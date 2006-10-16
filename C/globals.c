/*************************************************************************
*									 *
*	 YAP Prolog 							 *
*									 *
*	Yap Prolog was developed at NCCUP - Universidade do Porto	 *
*									 *
* Copyright L.Damas, V.S.Costa and Universidade do Porto 1985-1997	 *
*									 *
**************************************************************************
*									 *
* File:		non backtrackable term support				 *
* Last rev:	2/8/06							 *
* mods:									 *
* comments:	non-backtrackable term support				 *
*									 *
*************************************************************************/
#ifdef SCCS
static char SccsId[] = "%W% %G%";
#endif

#include "Yap.h"
#include "Yatom.h"
#include "Heap.h"
#include "yapio.h"
#include "iopreds.h"
#include "attvar.h"

/* Non-backtrackable terms will from now on be stored on arenas, a
   special term on the heap. Arenas automatically contract as we add terms to
   the front.

 */

#define QUEUE_ARENA 0
#define QUEUE_DELAY_ARENA 1
#define QUEUE_HEAD 2
#define QUEUE_TAIL 3
#define QUEUE_SIZE 4

#define HEAP_SIZE  0
#define HEAP_MAX   1
#define HEAP_ARENA 2
#define HEAP_DELAY_ARENA 3
#define HEAP_START 4

#define Global_MkIntegerTerm(I) MkIntegerTerm(I)

static UInt
big2arena_sz(CELL *arena_base)
{
  return ((MP_INT*)(arena_base+1))->_mp_alloc + (sizeof(MP_INT) + sizeof(Functor)+sizeof(CELL))/sizeof(CELL);
}

static UInt
arena2big_sz(UInt sz)
{
  return sz - (sizeof(MP_INT) + sizeof(Functor) + sizeof(CELL))/sizeof(CELL);
}


/* pointer to top of an arena */
static inline CELL *
ArenaLimit(Term arena)
{
  CELL *arena_base = RepAppl(arena);
  UInt sz = big2arena_sz(arena_base);
  return arena_base+sz;
}

/* pointer to top of an arena */
static inline CELL *
ArenaPt(Term arena)
{
  return (CELL *)RepAppl(arena);
}

static inline UInt
ArenaSz(Term arena)
{
  return big2arena_sz(RepAppl(arena));
}

static Term
CreateNewArena(CELL *ptr, UInt size)
{
    Term t = AbsAppl(ptr);
    MP_INT *dst;

    ptr[0] = (CELL)FunctorBigInt;
    dst = (MP_INT *)(ptr+1);
    dst->_mp_size = 0L;
    dst->_mp_alloc = arena2big_sz(size);
    ptr[size-1] = EndSpecials;

    return t;
}

/* pointer to top of an arena */
static inline attvar_record *
DelayArenaPt(Term arena)
{
  return (attvar_record *)arena;
}

static inline UInt
DelayArenaSz(Term arena)
{
  attvar_record *ptr = (attvar_record *)arena-1;
  return 1+(ptr-(attvar_record *)ptr->Done);
}

static void
ResetDelayArena(Term old_delay_arena, Term *new_arenap)
{
  attvar_record *min = (attvar_record *)*new_arenap;
  Term base = min[-1].Done;
  while (min < (attvar_record *)old_delay_arena) {
    min->Value = (Term)(min-1);
    min->Done = base;
    RESET_VARIABLE(&min->Atts);
    min++;
  }
  *new_arenap = old_delay_arena;
}


static Term
CreateDelayArena(attvar_record *max, attvar_record *min)
{
  attvar_record *ptr = max;
  while (ptr > min) {
    --ptr;
    ptr->Done = (CELL)min;
    ptr->Value = (CELL)(ptr-1);
    RESET_VARIABLE(&ptr->Atts);
  }
  RESET_VARIABLE(&(ptr->Value));
  return (CELL)max;
}

static Term
NewDelayArena(UInt size)
{
  attvar_record *max = DelayTop(), *min = max-size;
  Term out;

  while ((ADDR)min < Yap_GlobalBase+1024) {
    if (!Yap_InsertInGlobal((CELL *)max, size*sizeof(attvar_record))) {
      Yap_Error(OUT_OF_STACK_ERROR,TermNil,"No Stack Space for Non-Backtrackable terms");
      return TermNil;
    }
    max = DelayTop(), min = max-size;
  }
  out = CreateDelayArena(max, min);
  SetDelayTop(min);
  return out;
}

static Term
GrowDelayArena(Term *arenap, UInt old_size, UInt size, UInt arity)
{
  Term arena = *arenap;
  if (size == 0) {
    if (old_size < 1024) {
      size = old_size;
    } else {
      size = 1024;
    }
  }
  if (size < 64) {
    size = 64;
  }
  XREGS[arity+1] = (CELL)arenap;
  if (!Yap_InsertInGlobal((CELL *)arena, (size-old_size)*sizeof(attvar_record))) {
    Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
    return TermNil;
  }
  arenap = (CELL *)XREGS[arity+1];
  arena = *arenap;
  CreateDelayArena(DelayArenaPt(arena), DelayArenaPt(arena)-size);
  return arena;
}

static Term
NewArena(UInt size, UInt arity, CELL *where)
{
  Term t;

  if (where == NULL || where == H) {
    while (H+size > ASP-1024) {
      if (!Yap_gcl(size*sizeof(CELL), arity, ENV, P)) {
	Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
	return TermNil;
      }
    }
    t = CreateNewArena(H, size);
    H += size;
  } else {
    if (!Yap_InsertInGlobal(where, size*sizeof(CELL))) {
      Yap_Error(OUT_OF_STACK_ERROR,TermNil,"No Stack Space for Non-Backtrackable terms");
      return TermNil;
    }
    t = CreateNewArena(where, size);
  }
  return t;
}

static Int
p_allocate_arena(void)
{
  Term t = Deref(ARG1);
  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"allocate_arena");
    return FALSE;
  } else if (!IsIntegerTerm(t)) {
      Yap_Error(TYPE_ERROR_INTEGER,t,"allocate_arena");
      return FALSE;
  }
  return Yap_unify(ARG2,NewArena(IntegerOfTerm(t), 1, NULL));
}


static Int
p_default_arena_size(void)
{
  return Yap_unify(ARG1,MkIntegerTerm(ArenaSz(GlobalArena)));
}


static Int
p_allocate_default_arena(void)
{
  Term t = Deref(ARG1);
  Term t2 = Deref(ARG2);
  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"allocate_arena");
    return FALSE;
  } else if (!IsIntegerTerm(t)) {
      Yap_Error(TYPE_ERROR_INTEGER,t,"allocate_arena");
      return FALSE;
  }
  if (IsVarTerm(t2)) {
    Yap_Error(INSTANTIATION_ERROR,t2,"allocate_arena");
    return FALSE;
  } else if (!IsIntegerTerm(t)) {
      Yap_Error(TYPE_ERROR_INTEGER,t2,"allocate_arena");
      return FALSE;
  }
  GlobalArena = NewArena(IntegerOfTerm(t), 2, NULL);
  GlobalDelayArena = NewDelayArena(2);
  return TRUE;
}


static int
GrowArena(Term arena, CELL *pt, UInt old_size, UInt size, UInt arity)
{
  if (size == 0) {
    if (old_size < 64*1024) {
      size = old_size;
    } else {
      size = 64*1024;
    }
  }
  if (size < 4096) {
    size = 4096;
  }
  if (pt == H) {
    choiceptr b_ptr;
    if (H+size > ASP-1024) {

      XREGS[arity+1] = arena;
      if (!Yap_gcl(size*sizeof(CELL), arity+1, ENV, P)) {
	Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
      return FALSE;
      }
      arena = XREGS[arity+1];
    }
    /* adjust possible back pointers in choice-point stack */
    b_ptr = B;
    while (b_ptr->cp_h == H) {
      b_ptr->cp_h += size;
      b_ptr = b_ptr->cp_b;
    }
    H += size;
  } else {
    XREGS[arity+1] = arena;
    if (!Yap_InsertInGlobal(pt, size*sizeof(CELL))) {
      Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
      return FALSE;
    }
    arena = XREGS[arity+1];
  }
  CreateNewArena(ArenaPt(arena), size+old_size);
  return TRUE;
}

static void
CloseArena(CELL *oldH, CELL *oldHB, CELL *oldASP, Term *oldArenaP, UInt old_size)
{
  UInt new_size;

  if (H == oldH)
    return;
  new_size = old_size - (H-RepAppl(*oldArenaP));
  *oldArenaP = CreateNewArena(H, new_size);
  H = oldH;
  HB = oldHB;
  ASP = oldASP;
}

static inline void
clean_dirty_tr(tr_fr_ptr TR0) {
  if (TR != TR0) {
    tr_fr_ptr pt = TR0;

    do {
      Term p = TrailTerm(pt++);
      if (IsVarTerm(p)) {
	RESET_VARIABLE(p);
      } else {
	/* copy downwards */
	TrailTerm(TR0+1) = TrailTerm(pt);
	TrailTerm(TR0) = TrailTerm(TR0+2) = p;
	pt+=2;
	TR0 += 3;
      }
    } while (pt != TR);
    TR = TR0;
  }
}

static int
CopyAttVar(CELL *orig, CELL ***to_visit_ptr, CELL *res, Term *att_arenap)
{
  register attvar_record *attv = (attvar_record *)orig;
  register attvar_record *newv;
  CELL **to_visit = *to_visit_ptr;
  CELL *vt;

  /* add a new attributed variable */
  if (DelayArenaSz(*att_arenap) < 8)
    return FALSE;
  newv = DelayArenaPt(*att_arenap);
  newv--;
  RESET_VARIABLE(&(newv->Value));
  RESET_VARIABLE(&(newv->Done));
  vt = &(attv->Atts);
  to_visit[0] = vt-1;
  to_visit[1] = vt;
  if (IsVarTerm(attv->Atts)) {
    newv->Atts = (CELL)H;
    to_visit[2] = H;
    H++;
  } else {
    to_visit[2] = &(newv->Atts);
  }
  to_visit[3] = (CELL *)vt[-1];
  *to_visit_ptr = to_visit+4;
  *res = (CELL)&(newv->Done);
  *att_arenap = (CELL)(newv);
  return TRUE;
}

static int
copy_complex_term(register CELL *pt0, register CELL *pt0_end, CELL *ptf, CELL *HLow, Term *att_arenap)
{

  CELL **to_visit0, **to_visit = (CELL **)Yap_PreAllocCodeSpace();
  CELL *HB0 = HB;
  tr_fr_ptr TR0 = TR;
#ifdef COROUTINING
  CELL *dvars = NULL;
#endif
  HB = HLow;
  to_visit0 = to_visit;
 loop:
  while (pt0 < pt0_end) {
    register CELL d0;
    register CELL *ptd0;
    ++ pt0;
    ptd0 = pt0;
    d0 = *ptd0;
    deref_head(d0, copy_term_unk);
  copy_term_nvar:
    {
      if (IsPairTerm(d0)) {
	CELL *ap2 = RepPair(d0);
	if (ap2 >= HB && ap2 < H) {
	  /* If this is newer than the current term, just reuse */
	  *ptf++ = d0;
	  continue;
	} 
	*ptf = AbsPair(H);
	ptf++;
#ifdef RATIONAL_TREES
	if (to_visit + 4 >= (CELL **)AuxSp) {
	  goto heap_overflow;
	}
	to_visit[0] = pt0;
	to_visit[1] = pt0_end;
	to_visit[2] = ptf;
	to_visit[3] = (CELL *)*pt0;
	/* fool the system into thinking we had a variable there */
	*pt0 = AbsPair(H);
	to_visit += 4;
#else
	if (pt0 < pt0_end) {
	  if (to_visit + 3 >= (CELL **)AuxSp) {
	    goto heap_overflow;
	  }
	  to_visit[0] = pt0;
	  to_visit[1] = pt0_end;
	  to_visit[2] = ptf;
	  to_visit += 3;
	}
#endif
	pt0 = ap2 - 1;
	pt0_end = ap2 + 1;
	ptf = H;
	H += 2;
	if (H > ASP - 128) {
	  goto overflow;
	}
      } else if (IsApplTerm(d0)) {
	register Functor f;
	register CELL *ap2;
	/* store the terms to visit */
	ap2 = RepAppl(d0);
	if (ap2 >= HB && ap2 < H) {
	  /* If this is newer than the current term, just reuse */
	  *ptf++ = d0;
	  continue;
	} 
	f = (Functor)(*ap2);

	if (IsExtensionFunctor(f)) {
	  switch((CELL)f) {
	  case (CELL)FunctorDBRef:
	    *ptf++ = d0;
	    break;
	  case (CELL)FunctorLongInt:
	    if (H > ASP - (128+3)) {
	      goto overflow;
	    }
	    *ptf++ = AbsAppl(H);
	    H[0] = (CELL)f;
	    H[1] = ap2[1];
	    H[2] = EndSpecials;
	    H += 3;
	    break;
	  case (CELL)FunctorDouble:
	    if (H > ASP - (128+(2+SIZEOF_DOUBLE/sizeof(CELL)))) {
	      goto overflow;
	    }
	    *ptf++ = AbsAppl(H);
	    H[0] = (CELL)f;
	    H[1] = ap2[1];
#if SIZEOF_DOUBLE == 2*SIZEOF_LONG_INT
	    H[2] = ap2[2];
	    H[3] = EndSpecials;
	    H += 4;
#else
	    H[2] = EndSpecials;
	    H += 3;
#endif
	    break;
	  default:
	    {
	      /* big int */
	      UInt sz = ArenaSz(d0), i;

	      if (H > ASP - (128+sz)) {
		goto overflow;
	      }
	      *ptf++ = AbsAppl(H);
	      H[0] = (CELL)f;
	      for (i = 1; i < sz; i++) {
		H[i] = ap2[i];
	      }
	      H += sz;
	    }
	  }
	  continue;
	}
	*ptf = AbsAppl(H);
	ptf++;
	/* store the terms to visit */
#ifdef RATIONAL_TREES
	if (to_visit + 4 >= (CELL **)AuxSp) {
	  goto heap_overflow;
	}
	to_visit[0] = pt0;
	to_visit[1] = pt0_end;
	to_visit[2] = ptf;
	to_visit[3] = (CELL *)*pt0;
	/* fool the system into thinking we had a variable there */
	*pt0 = AbsAppl(H);
	to_visit += 4;
#else
	if (pt0 < pt0_end) {
	  if (to_visit + 3 >= (CELL **)AuxSp) {
	    goto heap_overflow;
	  }
	  to_visit[0] = pt0;
	  to_visit[1] = pt0_end;
	  to_visit[2] = ptf;
	  to_visit += 3;
	}
#endif
	d0 = ArityOfFunctor(f);
	pt0 = ap2;
	pt0_end = ap2 + d0;
	/* store the functor for the new term */
	H[0] = (CELL)f;
	ptf = H+1;
	H += 1+d0;
	if (H > ASP - 128) {
	  goto overflow;
	}
      } else {
	/* just copy atoms or integers */
	*ptf++ = d0;
      }
      continue;
    }

    derefa_body(d0, ptd0, copy_term_unk, copy_term_nvar);
    if (ptd0 >= HLow && ptd0 < H) { 
      /* we have already found this cell */
      *ptf++ = (CELL) ptd0;
    } else {
#if COROUTINING
      if (IsAttachedTerm((CELL)ptd0)) {
	/* if unbound, call the standard copy term routine */
	CELL **bp[1];

	if (dvars == NULL) {
	  dvars = (CELL *)DelayArenaPt(*att_arenap);
	} 	
	if (ptd0 < dvars &&
	    ptd0 >= (CELL *)DelayArenaPt(*att_arenap)) {
	  *ptf++ = (CELL) ptd0;
	} else {
	  tr_fr_ptr CurTR;

	  CurTR = TR;
	  bp[0] = to_visit;
	  HB = HB0;
	  if (!CopyAttVar(ptd0, bp, ptf, att_arenap)) {
	    goto delay_overflow;
	  }
	  if (H > ASP - 128) {
	    goto overflow;
	  }
	  to_visit = bp[0];
	  HB = HLow;
	  ptf++;
	  Bind_and_Trail(ptd0, ptf[-1]);
	}
      } else {
#endif
	/* first time we met this term */
	RESET_VARIABLE(ptf);
	Bind_and_Trail(ptd0, (CELL)ptf);
	ptf++;
#ifdef COROUTINING
      }
#endif
    }
  }
  /* Do we still have compound terms to visit */
  if (to_visit > to_visit0) {
#ifdef RATIONAL_TREES
    to_visit -= 4;
    pt0 = to_visit[0];
    pt0_end = to_visit[1];
    ptf = to_visit[2];
    *pt0 = (CELL)to_visit[3];
#else
    to_visit -= 3;
    pt0 = to_visit[0];
    pt0_end = to_visit[1];
    ptf = to_visit[2];
#endif
    goto loop;
  }

  /* restore our nice, friendly, term to its original state */
  HB = HB0;
  clean_dirty_tr(TR0);
  return 0;

 overflow:
  /* oops, we're in trouble */
  H = HLow;
  /* we've done it */
  /* restore our nice, friendly, term to its original state */
  HB = HB0;
#ifdef RATIONAL_TREES
  while (to_visit > to_visit0) {
    to_visit -= 4;
    pt0 = to_visit[0];
    pt0_end = to_visit[1];
    ptf = to_visit[2];
    *pt0 = (CELL)to_visit[3];
  }
#endif
  reset_trail(TR0);
  return -1;

 heap_overflow:
  /* oops, we're in trouble */
  H = HLow;
  /* we've done it */
  /* restore our nice, friendly, term to its original state */
  HB = HB0;
#ifdef RATIONAL_TREES
  while (to_visit > to_visit0) {
    to_visit -= 4;
    pt0 = to_visit[0];
    pt0_end = to_visit[1];
    ptf = to_visit[2];
    *pt0 = (CELL)to_visit[3];
  }
#endif
  reset_trail(TR0);
  return -2;

 delay_overflow:
  /* oops, we're in trouble */
  H = HLow;
  /* we've done it */
  /* restore our nice, friendly, term to its original state */
  HB = HB0;
#ifdef RATIONAL_TREES
  while (to_visit > to_visit0) {
    to_visit -= 4;
    pt0 = to_visit[0];
    pt0_end = to_visit[1];
    ptf = to_visit[2];
    *pt0 = (CELL)to_visit[3];
  }
#endif
  reset_trail(TR0);
  return -2;

}

static Term
CopyTermToArena(Term t, Term arena, UInt arity, Term *newarena, Term *att_arenap)
{
  UInt old_size = ArenaSz(arena);
  CELL *oldH = H;
  CELL *oldHB = HB;
  CELL *oldASP = ASP;
  int res;
  Term old_delay_arena;

 restart:
  old_delay_arena = *att_arenap;
  t = Deref(t);
  if (IsVarTerm(t)) {
    ASP = ArenaLimit(arena);
    H = HB = ArenaPt(arena);
#if COROUTINING
    if (IsAttachedTerm(t)) {
      CELL *Hi;

      *H = t;
      Hi = H+1;
      H += 2;
      if ((res = copy_complex_term(Hi-2, Hi-1, Hi, Hi, att_arenap)) < 0) 
	goto error_handler;
      CloseArena(oldH, oldHB, oldASP, newarena, old_size);
      return Hi[0];
    }
#endif
    Term tn = MkVarTerm();
    if (H > ASP - 128) {
      res = -1;
      goto error_handler;
    }
    CloseArena(oldH, oldHB, oldASP, newarena, old_size);
    return tn;
  } else if (IsAtomOrIntTerm(t)) {
    return t;
  } else if (IsPairTerm(t)) {
    Term tf;
    CELL *ap;
    CELL *Hi;

    H = HB = ArenaPt(arena);
    ASP = ArenaLimit(arena);
    ap = RepPair(t);
    Hi = H;
    tf = AbsPair(H);
    H += 2;
    if ((res = copy_complex_term(ap-1, ap+1, Hi, Hi, att_arenap)) < 0) {
	goto error_handler;
    }
    CloseArena(oldH, oldHB, oldASP, newarena, old_size);
    return tf;
  } else {
    Functor f;
    Term tf;
    CELL *HB0;
    CELL *ap;

    H = HB = ArenaPt(arena);
    ASP = ArenaLimit(arena);
    f = FunctorOfTerm(t);
    HB0 = H;
    ap = RepAppl(t);
    tf = AbsAppl(H);
    H[0] = (CELL)f;
    if (IsExtensionFunctor(f)) {
      switch((CELL)f) {
      case (CELL)FunctorDBRef:
	CloseArena(oldH, oldHB, oldASP, newarena, old_size);
	return t;
      case (CELL)FunctorLongInt:
	if (H > ASP - (128+3)) {
	  res = -1;
	  goto error_handler;
	}
	H[1] = ap[1];
	H[2] = EndSpecials;
	H += 3;
	break;
      case (CELL)FunctorDouble:
	if (H > ASP - (128+(2+SIZEOF_DOUBLE/sizeof(CELL)))) {
	  res = -1;
	  goto error_handler;
	}
	H[1] = ap[1];
#if SIZEOF_DOUBLE == 2*SIZEOF_LONG_INT
	H[2] = ap[2];
	H[3] = EndSpecials;
	H += 4;
#else
	H[2] = EndSpecials;
	H += 3;
#endif
	break;
      default:
	{
	  UInt sz = ArenaSz(t), i;

	  if (H > ASP - (128+sz)) {
	    res = -1;
	    goto error_handler;
	  }
	  for (i = 1; i < sz; i++) {
	    H[i] = ap[i];
	  }
	  H += sz;
	}
      }
    } else {
      H += 1+ArityOfFunctor(f);
      if ((res = copy_complex_term(ap, ap+ArityOfFunctor(f), HB0+1, HB0, att_arenap)) < 0) {
	goto error_handler;
      }
    }
    CloseArena(oldH, oldHB, oldASP, newarena, old_size);
    return tf;
  }
 error_handler:
  H = HB;
  CloseArena(oldH, oldHB, oldASP, newarena, old_size);
  if (old_delay_arena != MkIntTerm(0))
    ResetDelayArena(old_delay_arena, att_arenap);
  XREGS[arity+1] = t;
  XREGS[arity+2] = arena;
  XREGS[arity+3] = (CELL)newarena;
  XREGS[arity+4] = (CELL)att_arenap;
  {
    CELL *old_top = ArenaLimit(*newarena);
    ASP = oldASP;
    H = oldH;
    HB = oldHB;
    switch (res) {
    case -1:
      /* handle arena overflow */
      if (!GrowArena(arena, old_top, old_size, 0L, arity+4)) {
	Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
	return 0L;
      }
      break;
    case -2:
      /* handle delay arena overflow */
      old_size = DelayArenaSz(*att_arenap);
      if (!GrowDelayArena(att_arenap, old_size, 0L, arity+4)) {
	Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
	return 0L;
      }
      break;
    default: /* temporary space overflow */
      if (!Yap_ExpandPreAllocCodeSpace(0,NULL)) {
	Yap_Error(OUT_OF_AUXSPACE_ERROR, TermNil, Yap_ErrorMessage);
	return 0L;
      }
    }
  }
  oldH = H;
  oldHB = HB;
  oldASP = ASP;
  att_arenap = (Term *)XREGS[arity+4];
  newarena = (CELL *)XREGS[arity+3];
  arena = Deref(XREGS[arity+2]);
  t = XREGS[arity+1];
  old_size = ArenaSz(arena);
  goto restart;
}

inline static GlobalEntry *
FindGlobalEntry(Atom at)
/* get predicate entry for ap/arity; create it if neccessary.              */
{
  Prop p0;
  AtomEntry *ae = RepAtom(at);

  READ_LOCK(ae->ARWLock);
  p0 = ae->PropsOfAE;
  while (p0) {
    GlobalEntry *pe = RepGlobalProp(p0);
    if ( pe->KindOfPE == GlobalProperty
#if THREADS
	 && pe->owner_id == worker_id
#endif
	 ) {
      READ_UNLOCK(ae->ARWLock);
      return pe;
    }
    p0 = pe->NextOfPE;
  }
  READ_UNLOCK(ae->ARWLock);
  return NULL;
}

inline static GlobalEntry *
GetGlobalEntry(Atom at)
/* get predicate entry for ap/arity; create it if neccessary.              */
{
  Prop p0;
  AtomEntry *ae = RepAtom(at);
  GlobalEntry *new;

  WRITE_LOCK(ae->ARWLock);
  p0 = ae->PropsOfAE;
  while (p0) {
    GlobalEntry *pe = RepGlobalProp(p0);
    if ( pe->KindOfPE == GlobalProperty
#if THREADS
	 && pe->owner_id == worker_id
#endif
	 ) {
      WRITE_UNLOCK(ae->ARWLock);
      return pe;
    }
    p0 = pe->NextOfPE;
  }
  new = (GlobalEntry *) Yap_AllocAtomSpace(sizeof(*new));
  INIT_RWLOCK(new->GRWLock);
  new->KindOfPE = GlobalProperty;
#if THREADS
  new->owner_id = worker_id;
#endif
  new->NextGE = GlobalVariables;
  GlobalVariables = new;
  new->AtomOfGE = ae;
  new->NextOfPE = ae->PropsOfAE;
  ae->PropsOfAE = AbsGlobalProp(new);
  RESET_VARIABLE(&new->global);
  WRITE_UNLOCK(ae->ARWLock);
  return new;
}



static Int
p_nb_setval(void)
{
  Term t = Deref(ARG1), to;
  GlobalEntry *ge;
  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"nb_setval");
    return (TermNil);
  } else if (!IsAtomTerm(t)) {
      Yap_Error(TYPE_ERROR_ATOM,t,"nb_setval");
      return (FALSE);
  }
  ge = GetGlobalEntry(AtomOfTerm(t));
  to = CopyTermToArena(ARG2, GlobalArena, 2, &GlobalArena, &GlobalDelayArena);
  if (to == 0L)
    return FALSE;
  WRITE_LOCK(ge->GRWLock);
  ge->global=to;
  WRITE_UNLOCK(ge->GRWLock);
  return TRUE;
}

static Int
p_b_setval(void)
{
  Term t = Deref(ARG1);
  GlobalEntry *ge;

  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"b_setval");
    return (TermNil);
  } else if (!IsAtomTerm(t)) {
      Yap_Error(TYPE_ERROR_ATOM,t,"b_setval");
      return (FALSE);
  }
  ge = GetGlobalEntry(AtomOfTerm(t));
  WRITE_LOCK(ge->GRWLock);
#ifdef MULTI_ASSIGNMENT_VARIABLES
  /* the evil deed is to be done now */
  MaBind(&ge->global, ARG2);
  WRITE_UNLOCK(ge->GRWLock);
  return TRUE;
#else
  WRITE_UNLOCK(ge->GRWLock);
  Yap_Error(SYSTEM_ERROR,t2,"update_array");
  return FALSE;
#endif
}

static Int
p_nb_getval(void)
{
  Term t = Deref(ARG1), to;
  GlobalEntry *ge;

  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"nb_getval");
    return FALSE;
  } else if (!IsAtomTerm(t)) {
    Yap_Error(TYPE_ERROR_ATOM,t,"nb_getval");
    return FALSE;
  }
  ge = FindGlobalEntry(AtomOfTerm(t));
  if (!ge)
    return FALSE;
  READ_LOCK(ge->GRWLock);
  to = ge->global;
  READ_UNLOCK(ge->GRWLock);
  return Yap_unify(ARG2, to);
}

static Int
p_nb_delete(void)
{
  Term t = Deref(ARG1);
  GlobalEntry *ge, *g;
  AtomEntry *ae;
  Prop gp, g0;

  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"nb_delete");
    return FALSE;
  } else if (!IsAtomTerm(t)) {
    Yap_Error(TYPE_ERROR_ATOM,t,"nb_delete");
    return FALSE;
  }
  ge = FindGlobalEntry(AtomOfTerm(t));
  if (!ge)
    return FALSE;
  WRITE_LOCK(ge->GRWLock);
  ae = ge->AtomOfGE;
  if (GlobalVariables == ge) {
    GlobalVariables = ge->NextGE;
  } else {
    g = GlobalVariables;
    while (g->NextGE != ge) 
      g = g->NextGE;
    g->NextGE = ge->NextGE;
  }
  gp = AbsGlobalProp(ge);
  WRITE_LOCK(ae->ARWLock);
  if (ae->PropsOfAE == gp) {
    ae->PropsOfAE = ge->NextOfPE;
  } else {
    g0 = ae->PropsOfAE;
    while (g0->NextOfPE != gp) 
      g0 = g0->NextOfPE;
    g0->NextOfPE = ge->NextOfPE;
  }
  WRITE_UNLOCK(ae->ARWLock);
  WRITE_UNLOCK(ge->GRWLock);
  Yap_FreeCodeSpace((char *)ge);
  return FALSE;
}

/* a non-backtrackable queue is a term of the form $array(Arena,Start,End,Size) plus an Arena. */

static Int
p_nb_queue(void)
{
  Term queue_arena, delay_queue_arena, queue, ar[5], *nar;
  Term t = Deref(ARG1);
  UInt arena_sz = (H-H0)/16;

  if (!IsVarTerm(t)) {
    if (!IsApplTerm(t)) {
      return FALSE;
    }
    return (FunctorOfTerm(t) == FunctorNBQueue);
  }
  ar[QUEUE_ARENA] = 
    ar[QUEUE_DELAY_ARENA] = 
    ar[QUEUE_HEAD] =
    ar[QUEUE_TAIL] =
    ar[QUEUE_SIZE] =
    MkIntTerm(0);
  queue = Yap_MkApplTerm(FunctorNBQueue,5,ar);
  if (!Yap_unify(queue,ARG1))
    return FALSE;
  arena_sz = ((attvar_record *)H0- DelayTop())/16;
  if (arena_sz <2) 
    arena_sz = 2;
  if (arena_sz > 256)
      arena_sz = 256;
  delay_queue_arena = NewDelayArena(arena_sz);
  if (delay_queue_arena == 0L) {
    return FALSE;
  }
  nar = RepAppl(Deref(ARG1))+1;
  nar[QUEUE_DELAY_ARENA] = delay_queue_arena;
  if (arena_sz < 4*1024)
    arena_sz = 4*1024;
  queue_arena = NewArena(arena_sz,1,NULL);
  if (queue_arena == 0L) {
    return FALSE;
  }
  nar = RepAppl(Deref(ARG1))+1;
  nar[QUEUE_ARENA] = queue_arena;
  return TRUE;
}

static CELL *
GetQueue(Term t, char* caller)
{
  t = Deref(t);

  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,caller);
    return NULL;
  } 
  if (!IsApplTerm(t)) {
      Yap_Error(TYPE_ERROR_COMPOUND,t,caller);
      return NULL;
  }
  if (FunctorOfTerm(t) != FunctorNBQueue) {
      Yap_Error(DOMAIN_ERROR_ARRAY_TYPE,t,caller);
      return NULL;
  }
  return RepAppl(t)+1;
}

static Term
GetQueueArena(CELL *qd, char* caller)
{
  Term t = Deref(qd[QUEUE_ARENA]);

  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,caller);
    return 0L;
  } 
  if (!IsApplTerm(t)) {
      Yap_Error(TYPE_ERROR_COMPOUND,t,caller);
      return 0L;
  }
  if (FunctorOfTerm(t) != FunctorBigInt) {
      Yap_Error(DOMAIN_ERROR_ARRAY_TYPE,t,caller);
      return 0L;
  }
  return t;
}

static void
RecoverDelayArena(Term delay_arena)
{
  attvar_record *pt = DelayArenaPt(delay_arena),
    *max = DelayTop();
  
  if (max == pt-DelayArenaSz(delay_arena))
      SetDelayTop(pt);
}

static void
RecoverArena(Term arena)
{
  CELL *pt = ArenaPt(arena),
    *max = ArenaLimit(arena);
  
  if (max == H)
    H = pt;
}

static Int
p_nb_queue_close(void)
{
  Term t = Deref(ARG1);
  Int out;

  if (!IsVarTerm(t)) {
    CELL *qp;

    qp = GetQueue(t, "queue/3");
    if (qp == NULL) {
      return
	Yap_unify(ARG3, ARG2);
    }
    if (qp[QUEUE_ARENA] != MkIntTerm(0))
      RecoverArena(qp[QUEUE_ARENA]);
    if (qp[QUEUE_DELAY_ARENA] != MkIntTerm(0))
      RecoverDelayArena(qp[QUEUE_DELAY_ARENA]);
    if (qp[QUEUE_SIZE] == MkIntTerm(0)) {
      return 
	Yap_unify(ARG3, ARG2);
    }
    out = 
      Yap_unify(ARG3, qp[QUEUE_TAIL]) &&
      Yap_unify(ARG2, qp[QUEUE_HEAD]);
    qp[-1] = (CELL)Yap_MkFunctor(Yap_LookupAtom("heap"),1);
    qp[0] = MkIntegerTerm(0);
    return out;
  }
  Yap_Error(INSTANTIATION_ERROR,t,"queue/3");
  return FALSE;
}

static Int
p_nb_queue_enqueue(void)
{
  CELL *qd = GetQueue(ARG1,"enqueue"), *oldH, *oldHB;
  UInt old_sz;
  Term arena, qsize, to;

  if (!qd)
    return FALSE;
  arena = GetQueueArena(qd,"enqueue");
  if (arena == 0L)
    return FALSE;
  to = CopyTermToArena(ARG2, arena, 2, qd+QUEUE_ARENA, qd+QUEUE_DELAY_ARENA);
  if (to == 0L)
    return FALSE;
  qd = GetQueue(ARG1,"enqueue");
  arena = GetQueueArena(qd,"enqueue");
  /* garbage collection ? */
  oldH = H;
  oldHB = HB;
  H = HB = ArenaPt(arena);
  old_sz = ArenaSz(arena);
  qsize = IntegerOfTerm(qd[QUEUE_SIZE]);
  while (old_sz < 128) {
    UInt gsiz = qsize*2;

    H = oldH;
    HB = oldHB;
    if (gsiz > 1024*1024) {
      gsiz = 1024*1024;
    } else if (gsiz < 1024) {
      gsiz = 1024;
    }
    ARG3 = to;
    if (!GrowArena(arena, ArenaLimit(arena), old_sz, gsiz, 3)) {
      Yap_Error(OUT_OF_STACK_ERROR, arena, Yap_ErrorMessage);
      return 0L;
    }    
    to = ARG3;
    qd = RepAppl(Deref(ARG1))+1;
    arena = GetQueueArena(qd,"enqueue");
    oldH = H;
    oldHB = HB;
    H = HB = ArenaPt(arena);
    old_sz = ArenaSz(arena);    
  }
  qd[QUEUE_SIZE] = Global_MkIntegerTerm(qsize+1);
  if (qsize == 0) {
    qd[QUEUE_HEAD] = AbsPair(H);
  } else {
    *VarOfTerm(qd[QUEUE_TAIL]) = AbsPair(H);
  }
  *H++ = to;
  RESET_VARIABLE(H);
  qd[QUEUE_TAIL] = (CELL)H;
  H++;
  CloseArena(oldH, oldHB, ASP, qd+QUEUE_ARENA, old_sz);
  return TRUE;
}

static Int
p_nb_queue_dequeue(void)
{
  CELL *qd = GetQueue(ARG1,"dequeue");
  UInt old_sz, qsz;
  Term arena, out;
  CELL *oldH, *oldHB;

  if (!qd)
    return FALSE;
  qsz = IntegerOfTerm(qd[QUEUE_SIZE]);
  if (qsz == 0)
    return FALSE;
  arena = GetQueueArena(qd,"dequeue");
  if (arena == 0L)
    return FALSE;
  old_sz = ArenaSz(arena);
  out = HeadOfTerm(qd[QUEUE_HEAD]);
  qd[QUEUE_HEAD] = TailOfTerm(qd[QUEUE_HEAD]);
  /* garbage collection ? */
  oldH = H;
  oldHB = HB;
  qd[QUEUE_SIZE] = Global_MkIntegerTerm(qsz-1);
  CloseArena(oldH, oldHB, ASP, &arena, old_sz);
  return Yap_unify(out, ARG2);
}

static Int
p_nb_queue_peek(void)
{
  CELL *qd = GetQueue(ARG1,"queue_peek");
  UInt qsz;

  if (!qd)
    return FALSE;
  qsz = IntegerOfTerm(qd[QUEUE_SIZE]);
  if (qsz == 0)
    return FALSE;
  return Yap_unify(HeadOfTerm(qd[QUEUE_HEAD]), ARG2);
}

static Int
p_nb_queue_empty(void)
{
  CELL *qd = GetQueue(ARG1,"queue_empty");

  if (!qd)
    return FALSE;
  return (IntegerOfTerm(qd[QUEUE_SIZE]) == 0);
}

static Int
p_nb_queue_size(void)
{
  CELL *qd = GetQueue(ARG1,"queue_size");

  if (!qd)
    return FALSE;
  return Yap_unify(ARG2,qd[QUEUE_SIZE]);
}


static CELL *
GetHeap(Term t, char* caller)
{
  t = Deref(t);

  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,caller);
    return NULL;
  } 
  if (!IsApplTerm(t)) {
      Yap_Error(TYPE_ERROR_COMPOUND,t,caller);
      return NULL;
  }
  return RepAppl(t)+1;
}

static Term
MkZeroApplTerm(Functor f, UInt sz)
{
  Term t0, tf;
  CELL *pt;

  if (H+(sz+1) > ASP-1024)
    return TermNil;
  tf = AbsAppl(H);
  *H = (CELL)f;
  t0 = MkIntTerm(0);
  pt = H+1;
  while (sz--) {
    *pt++ = t0;
  }
  H = pt;
  return tf;
}

static Int
p_nb_heap(void)
{
  Term heap_arena, delay_heap_arena, heap, *ar, *nar;
  UInt hsize;
  Term tsize = Deref(ARG1);
  UInt arena_sz = (H-H0)/16;

  if (IsVarTerm(tsize)) {
    Yap_Error(INSTANTIATION_ERROR,tsize,"nb_heap");
    return FALSE;
  } else {
    if (!IsIntegerTerm(tsize)) {
      Yap_Error(TYPE_ERROR_INTEGER,tsize,"nb_heap");
      return FALSE;
    }
    hsize = IntegerOfTerm(tsize);
  }

  while ((heap = MkZeroApplTerm(Yap_MkFunctor(Yap_LookupAtom("heap"),2*hsize+HEAP_START+1),2*hsize+HEAP_START+1)) == TermNil) {
    if (!Yap_gcl((2*hsize+HEAP_START+1)*sizeof(CELL), 2, ENV, P)) {
      Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
      return FALSE;
    }
  }
  if (!Yap_unify(heap,ARG2))
    return FALSE;
  ar = RepAppl(heap)+1;
  ar[HEAP_ARENA] = 
    ar[HEAP_DELAY_ARENA] = 
    ar[HEAP_SIZE] = 
    MkIntTerm(0);
  ar[HEAP_MAX] = tsize;
  if (arena_sz < 1024)
    arena_sz = 1024;
  heap_arena = NewArena(arena_sz,1,NULL);
  if (heap_arena == 0L) {
    return FALSE;
  }
  nar = RepAppl(Deref(ARG2))+1;
  nar[HEAP_ARENA] = heap_arena;
  arena_sz = ((attvar_record *)H0- DelayTop())/16;
  if (arena_sz <2) 
    arena_sz = 2;
  if (arena_sz > 256)
      arena_sz = 256;
  delay_heap_arena = NewDelayArena(arena_sz);
  if (delay_heap_arena == 0L) {
    return FALSE;
  }
  nar = RepAppl(Deref(ARG2))+1;
  nar[HEAP_DELAY_ARENA] = delay_heap_arena;
  return TRUE;
}

static Int
p_nb_heap_close(void)
{
  Term t = Deref(ARG1);
  if (!IsVarTerm(t)) {
    CELL *qp;

    qp = RepAppl(t)+1;
    if (qp[HEAP_ARENA] != MkIntTerm(0))
      RecoverArena(qp[HEAP_ARENA]);
    if (qp[HEAP_DELAY_ARENA] != MkIntTerm(0))
      RecoverDelayArena(qp[HEAP_DELAY_ARENA]);
    qp[-1] = (CELL)Yap_MkFunctor(Yap_LookupAtom("heap"),1);
    qp[0] = MkIntegerTerm(0);
    return TRUE;
  }
  Yap_Error(INSTANTIATION_ERROR,t,"heap_close/1");
  return FALSE;
}

static void
PushHeap(CELL *pt, UInt off)
{
  while (off) {
    UInt noff = (off+1)/2-1;
    if (Yap_compare_terms(pt[2*off], pt[2*noff]) < 0) {
      Term tk = pt[2*noff];
      Term tv = pt[2*noff+1];
      pt[2*noff] = pt[2*off];
      pt[2*noff+1] = pt[2*off+1];
      pt[2*off] = tk;
      pt[2*off+1] = tv;
      off = noff;
    } else {
      return;
    }
  }
}

static void
DelHeapRoot(CELL *pt, UInt sz)
{
  UInt indx = 0;
  Term tk, tv;

  sz--;
  tk = pt[2*sz];
  tv = pt[2*sz+1];
  while (TRUE) {
    if (sz < 2*indx+3 || Yap_compare_terms(pt[4*indx+2],pt[4*indx+4]) < 0) {
      if (sz < 2*indx+2 || Yap_compare_terms(tk, pt[4*indx+2]) < 0) {
	pt[2*indx] = tk;
	pt[2*indx+1] = tv;
	return;
      } else {
	pt[2*indx] = pt[4*indx+2];
	pt[2*indx+1] = pt[4*indx+3];
	indx = 2*indx+1;
      }
    } else {
      if (Yap_compare_terms(tk, pt[4*indx+4]) < 0) {
	pt[2*indx] = tk;
	pt[2*indx+1] = tv;
	return;
      } else {
	pt[2*indx] = pt[4*indx+4];
	pt[2*indx+1] = pt[4*indx+5];
	indx = 2*indx+2;
      }
    }
  }
}

static Int
p_nb_heap_add_to_heap(void)
{
  CELL *qd = GetHeap(ARG1,"add_to_heap"), *oldH, *oldHB, *pt;
  UInt hsize, hmsize, old_sz;
  Term arena, to, key;

  if (!qd)
    return FALSE;
 restart:
  hsize = IntegerOfTerm(qd[HEAP_SIZE]);
  hmsize = IntegerOfTerm(qd[HEAP_MAX]);
  if (hsize == hmsize) {
    CELL *top = qd+(HEAP_START+2*hmsize);
    UInt extra_size;

    if (hmsize >= 64*1024) {
      extra_size = 64*1024;
    } else {
      extra_size = hmsize;
    }
    if (!Yap_InsertInGlobal(top, extra_size*2*sizeof(CELL))) {
      Yap_Error(OUT_OF_STACK_ERROR,TermNil,"No Stack Space for Non-Backtrackable terms");
      return FALSE;
    }
    qd = GetHeap(ARG1,"add_to_heap");
    hmsize += extra_size;
    if (!qd)
      return FALSE;
    qd[-1] = (CELL)Yap_MkFunctor(Yap_LookupAtom("heap"),2*hmsize+HEAP_START)+1;
    top = qd+(HEAP_START+2*(hmsize-extra_size));
    while (extra_size) {
      RESET_VARIABLE(top);
      RESET_VARIABLE(top+1);
      top+=2;
      extra_size--;
    }
    arena = qd[HEAP_ARENA];
    old_sz = ArenaSz(arena);
    oldH = H;
    oldHB = HB;
    H = HB = ArenaPt(arena);
    qd[HEAP_MAX] = Global_MkIntegerTerm(hmsize);
    CloseArena(oldH, oldHB, ASP, qd+HEAP_ARENA, old_sz);
    goto restart;
  }
  arena = qd[HEAP_ARENA];
  if (arena == 0L)
    return FALSE;
  key = CopyTermToArena(ARG2, arena, 3, qd+HEAP_ARENA, qd+HEAP_DELAY_ARENA);
  arena = qd[HEAP_ARENA];
  to = CopyTermToArena(ARG3, arena, 3, qd+HEAP_ARENA, qd+HEAP_DELAY_ARENA);
  if (key == 0 || to == 0L)
    return FALSE;
  qd = GetHeap(ARG1,"add_to_heap");
  arena = qd[HEAP_ARENA];
  /* garbage collection ? */
  oldH = H;
  oldHB = HB;
  H = HB = ArenaPt(arena);
  old_sz = ArenaSz(arena);
  while (old_sz < 128) {
    UInt gsiz = hsize*2;

    H = oldH;
    HB = oldHB;
    if (gsiz > 1024*1024) {
      gsiz = 1024*1024;
    } else if (gsiz < 1024) {
      gsiz = 1024;
    }
    ARG3 = to;
    if (!GrowArena(arena, ArenaLimit(arena), old_sz, gsiz, 3)) {
      Yap_Error(OUT_OF_STACK_ERROR, arena, Yap_ErrorMessage);
      return 0L;
    }    
    to = ARG3;
    qd = RepAppl(Deref(ARG1))+1;
    arena = qd[HEAP_ARENA];
    oldH = H;
    oldHB = HB;
    H = HB = ArenaPt(arena);
    old_sz = ArenaSz(arena);    
  }
  pt = qd+HEAP_START;
  pt[2*hsize] = key;
  pt[2*hsize+1] = to;
  PushHeap(pt, hsize);
  qd[HEAP_SIZE] = Global_MkIntegerTerm(hsize+1);
  CloseArena(oldH, oldHB, ASP, qd+HEAP_ARENA, old_sz);
  return TRUE;
}

static Int
p_nb_heap_del(void)
{
  CELL *qd = GetHeap(ARG1,"deheap");
  UInt old_sz, qsz;
  Term arena;
  CELL *oldH, *oldHB;
  Term tk, tv;

  if (!qd)
    return FALSE;
  qsz = IntegerOfTerm(qd[HEAP_SIZE]);
  if (qsz == 0)
    return FALSE;
  arena = qd[HEAP_ARENA];
  if (arena == 0L)
    return FALSE;
  old_sz = ArenaSz(arena);
  /* garbage collection ? */
  oldH = H;
  oldHB = HB;
  qd[HEAP_SIZE] = Global_MkIntegerTerm(qsz-1);
  CloseArena(oldH, oldHB, ASP, &arena, old_sz);
  tk = qd[HEAP_START];
  tv = qd[HEAP_START+1];
  DelHeapRoot(qd+HEAP_START, qsz);
  return Yap_unify(tk, ARG2) &&
    Yap_unify(tv, ARG3);
}

static Int
p_nb_heap_peek(void)
{
  CELL *qd = GetHeap(ARG1,"heap_peek");
  UInt qsz;
  Term tk, tv;

  if (!qd)
    return FALSE;
  qsz = IntegerOfTerm(qd[HEAP_SIZE]);
  if (qsz == 0)
    return FALSE;
  tk = qd[HEAP_START];
  tv = qd[HEAP_START+1];
  return Yap_unify(tk, ARG2) &&
    Yap_unify(tv, ARG3);
}

static Int
p_nb_heap_empty(void)
{
  CELL *qd = GetHeap(ARG1,"heap_empty");

  if (!qd)
    return FALSE;
  return (IntegerOfTerm(qd[HEAP_SIZE]) == 0);
}

static Int
p_nb_heap_size(void)
{
  CELL *qd = GetHeap(ARG1,"heap_size");

  if (!qd)
    return FALSE;
  return Yap_unify(ARG2,qd[HEAP_SIZE]);
}

static Int
p_nb_beam(void)
{
  Term beam_arena, delay_beam_arena, beam, *ar, *nar;
  UInt hsize;
  Term tsize = Deref(ARG1);
  UInt arena_sz = (H-H0)/16;

  if (IsVarTerm(tsize)) {
    Yap_Error(INSTANTIATION_ERROR,tsize,"nb_beam");
    return FALSE;
  } else {
    if (!IsIntegerTerm(tsize)) {
      Yap_Error(TYPE_ERROR_INTEGER,tsize,"nb_beam");
      return FALSE;
    }
    hsize = IntegerOfTerm(tsize);
  }
  while ((beam = MkZeroApplTerm(Yap_MkFunctor(Yap_LookupAtom("heap"),5*hsize+HEAP_START+1),5*hsize+HEAP_START+1)) == TermNil) {
    if (!Yap_gcl((5*hsize+HEAP_START+1)*sizeof(CELL), 2, ENV, P)) {
      Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
      return FALSE;
    }
  }
  if (!Yap_unify(beam,ARG2))
    return FALSE;
  ar = RepAppl(beam)+1;
  ar[HEAP_ARENA] = 
    ar[HEAP_DELAY_ARENA] = 
    ar[HEAP_SIZE] = 
    MkIntTerm(0);
  ar[HEAP_MAX] = tsize;
  if (arena_sz < 1024)
    arena_sz = 1024;
  beam_arena = NewArena(arena_sz,1,NULL);
  if (beam_arena == 0L) {
    return FALSE;
  }
  nar = RepAppl(Deref(ARG2))+1;
  nar[HEAP_ARENA] = beam_arena;
  arena_sz = ((attvar_record *)H0- DelayTop())/16;
  if (arena_sz <2) 
    arena_sz = 2;
  if (arena_sz > 256)
      arena_sz = 256;
  delay_beam_arena = NewDelayArena(arena_sz);
  if (delay_beam_arena == 0L) {
    return FALSE;
  }
  nar = RepAppl(Deref(ARG2))+1;
  nar[HEAP_DELAY_ARENA] = delay_beam_arena;
  return TRUE;
}

static Int
p_nb_beam_close(void)
{
  return p_nb_beam_close();
}


/* we have two queues, one with
   Key, IndxQueue2
   the other with
   Key, IndxQueue1, Val
*/
static void
PushBeam(CELL *pt, CELL *npt, UInt hsize, Term key, Term to)
{
  UInt off = hsize, off2 = hsize;
  Term toff, toff2;

  /* push into first queue */
  while (off) {
    UInt noff = (off+1)/2-1;
    if (Yap_compare_terms(key, pt[2*noff]) < 0) {
      UInt i2 = IntegerOfTerm(pt[2*noff+1]);

      pt[2*off] = pt[2*noff];
      pt[2*off+1] = pt[2*noff+1];
      npt[3*i2+1] = Global_MkIntegerTerm(off);
      off = noff;
    } else {
      break;
    }
  }
  toff = Global_MkIntegerTerm(off);
  /* off says where we are in first queue */
  /* push into second queue */
  while (off2) {
    UInt noff = (off2+1)/2-1;
    if (Yap_compare_terms(key, npt[3*noff]) > 0) {
      UInt i1 = IntegerOfTerm(npt[3*noff+1]);

      npt[3*off2] = npt[3*noff];
      npt[3*off2+1] = npt[3*noff+1];
      npt[3*off2+2] = npt[3*noff+2];
      pt[2*i1+1] = Global_MkIntegerTerm(off2);
      off2 = noff;
    } else {
      break;
    }
  }
  toff2 = Global_MkIntegerTerm(off2);
  /* store elements in their rightful place */
  npt[3*off2] = pt[2*off] = key;
  pt[2*off+1] = toff2;
  npt[3*off2+1] = toff;
  npt[3*off2+2] = to;
}

static void
DelBeamMax(CELL *pt, CELL *pt2, UInt sz)
{
  UInt off = IntegerOfTerm(pt2[1]);
  UInt indx = 0;
  Term tk, ti, tv;

  sz--;
  /* first, fix the reverse queue */
  tk = pt2[3*sz];
  ti = pt2[3*sz+1];
  tv = pt2[3*sz+2];
  while (TRUE) {
    if (sz < 2*indx+3 || Yap_compare_terms(pt2[6*indx+3],pt2[6*indx+6]) > 0) {
      if (sz < 2*indx+2 || Yap_compare_terms(tk, pt2[6*indx+3]) > 0) {
	break;
      } else {
	UInt off = IntegerOfTerm(pt2[6*indx+4]);

	pt2[3*indx] = pt2[6*indx+3];
	pt2[3*indx+1] = pt2[6*indx+4];
	pt2[3*indx+2] = pt2[6*indx+5];
	pt[2*off+1] = Global_MkIntegerTerm(indx);
	indx = 2*indx+1;
      }
    } else {
      if (Yap_compare_terms(tk, pt2[6*indx+6]) > 0) {
	break;
      } else {
	UInt off = IntegerOfTerm(pt2[6*indx+7]);

	pt2[3*indx] = pt2[6*indx+6];
	pt2[3*indx+1] = pt2[6*indx+7];
	pt2[3*indx+2] = pt2[6*indx+8];
	pt[2*off+1] = Global_MkIntegerTerm(indx);
	indx = 2*indx+2;
      }
    }
  }
  pt[2*IntegerOfTerm(ti)+1] = Global_MkIntegerTerm(indx);
  pt2[3*indx] = tk;
  pt2[3*indx+1] = ti;
  pt2[3*indx+2] = tv;
  /* now, fix the standard queue */
  if (off != sz) {
    Term toff, toff2, key;
    UInt off2;

    key = pt[2*sz];
    toff2 = pt[2*sz+1];
    off2 = IntegerOfTerm(toff2);
    /* off says where we are in first queue */
    /* push into second queue */
    while (off) {
      UInt noff = (off+1)/2-1;
      if (Yap_compare_terms(key, pt[2*noff]) < 0) {
	UInt i1 = IntegerOfTerm(pt[2*noff+1]);

	pt[2*off] = pt[2*noff];
	pt[2*off+1] = pt[2*noff+1];
	pt2[3*i1+1] = Global_MkIntegerTerm(off);
	off = noff;
      } else {
	break;
      }
    }
    toff = Global_MkIntegerTerm(off);
    /* store elements in their rightful place */
    pt[2*off] = key;
    pt2[3*off2+1] = toff;
    pt[2*off+1] = toff2;
  }
}

static Term
DelBeamMin(CELL *pt, CELL *pt2, UInt sz)
{
  UInt off2 = IntegerOfTerm(pt[1]);
  Term ov = pt2[3*off2+2]; /* return value */
  UInt indx = 0;
  Term tk, tv;

  sz--;
  /* first, fix the standard queue */
  tk = pt[2*sz];
  tv = pt[2*sz+1];
  while (TRUE) {
    if (sz < 2*indx+3 || Yap_compare_terms(pt[4*indx+2],pt[4*indx+4]) < 0) {
      if (sz < 2*indx+2 || Yap_compare_terms(tk, pt[4*indx+2]) < 0) {
	break;
      } else {
	UInt off2 = IntegerOfTerm(pt[4*indx+3]);
	pt[2*indx] = pt[4*indx+2];
	pt[2*indx+1] = pt[4*indx+3];
	pt2[3*off2+1] = Global_MkIntegerTerm(indx);
	indx = 2*indx+1;
      }
    } else {
      if (Yap_compare_terms(tk, pt[4*indx+4]) < 0) {
	break;
      } else {
	UInt off2 = IntegerOfTerm(pt[4*indx+5]);

	pt[2*indx] = pt[4*indx+4];
	pt[2*indx+1] = pt[4*indx+5];
	pt2[3*off2+1] = Global_MkIntegerTerm(indx);
	indx = 2*indx+2;
      }
    }
  }
  pt[2*indx] = tk;
  pt[2*indx+1] = tv;
  pt2[3*IntegerOfTerm(tv)+1] = Global_MkIntegerTerm(indx);
  /* now, fix the reverse queue */
  if (off2 != sz) {
    Term to, toff, toff2, key;
    UInt off;

    key = pt2[3*sz];
    toff = pt2[3*sz+1];
    to = pt2[3*sz+2];
    off = IntegerOfTerm(toff);
    /* off says where we are in first queue */
    /* push into second queue */
    while (off2) {
      UInt noff = (off2+1)/2-1;
      if (Yap_compare_terms(key, pt2[3*noff]) > 0) {
	UInt i1 = IntegerOfTerm(pt2[3*noff+1]);

	pt2[3*off2] = pt2[3*noff];
	pt2[3*off2+1] = pt2[3*noff+1];
	pt2[3*off2+2] = pt2[3*noff+2];
	pt[2*i1+1] = Global_MkIntegerTerm(off2);
	off2 = noff;
      } else {
	break;
      }
    }
    toff2 = Global_MkIntegerTerm(off2);
    /* store elements in their rightful place */
    pt2[3*off2] = key;
    pt[2*off+1] = toff2;
    pt2[3*off2+1] = toff;
    pt2[3*off2+2] = to;
  }
  return ov;
}

static Int
p_nb_beam_add_to_beam(void)
{
  CELL *qd = GetHeap(ARG1,"add_to_beam"), *oldH, *oldHB, *pt;
  UInt hsize, hmsize, old_sz;
  Term arena, to, key;

  if (!qd)
    return FALSE;
  hsize = IntegerOfTerm(qd[HEAP_SIZE]);
  hmsize = IntegerOfTerm(qd[HEAP_MAX]);
  key = Deref(ARG2);
  if (hsize == hmsize) {
    pt = qd+HEAP_START;
    if (Yap_compare_terms(pt[2*hmsize],Deref(ARG2)) > 0) {
      /* smaller than current max, we need to drop current max */
      DelBeamMax(pt, pt+2*hmsize, hmsize);
      hsize--;
    } else {
      return TRUE;
    }
  }
  arena = qd[HEAP_ARENA];
  if (arena == 0L)
    return FALSE;
  key = CopyTermToArena(ARG2, qd[HEAP_ARENA], 3, qd+HEAP_ARENA, qd+HEAP_DELAY_ARENA);
  arena = qd[HEAP_ARENA];
  to = CopyTermToArena(ARG3, arena, 3, qd+HEAP_ARENA, qd+HEAP_DELAY_ARENA);
  if (key == 0 || to == 0L)
    return FALSE;
  qd = GetHeap(ARG1,"add_to_beam");
  arena = qd[HEAP_ARENA];
  /* garbage collection ? */
  oldH = H;
  oldHB = HB;
  H = HB = ArenaPt(arena);
  old_sz = ArenaSz(arena);
  while (old_sz < 128) {
    UInt gsiz = hsize*2;

    H = oldH;
    HB = oldHB;
    if (gsiz > 1024*1024) {
      gsiz = 1024*1024;
    } else if (gsiz < 1024) {
      gsiz = 1024;
    }
    ARG3 = to;
    if (!GrowArena(arena, ArenaLimit(arena), old_sz, gsiz, 3)) {
      Yap_Error(OUT_OF_STACK_ERROR, arena, Yap_ErrorMessage);
      return 0L;
    }    
    to = ARG3;
    qd = RepAppl(Deref(ARG1))+1;
    arena = qd[HEAP_ARENA];
    oldH = H;
    oldHB = HB;
    H = HB = ArenaPt(arena);
    old_sz = ArenaSz(arena);    
  }
  pt = qd+HEAP_START;
  PushBeam(pt, pt+2*hmsize, hsize, key, to);
  qd[HEAP_SIZE] = Global_MkIntegerTerm(hsize+1);
  CloseArena(oldH, oldHB, ASP, qd+HEAP_ARENA, old_sz);
  return TRUE;
}

static Int
p_nb_beam_del(void)
{
  CELL *qd = GetHeap(ARG1,"debeam");
  UInt old_sz, qsz;
  Term arena;
  CELL *oldH, *oldHB;
  Term tk, tv;

  if (!qd)
    return FALSE;
  qsz = IntegerOfTerm(qd[HEAP_SIZE]);
  if (qsz == 0)
    return FALSE;
  arena = qd[HEAP_ARENA];
  if (arena == 0L)
    return FALSE;
  old_sz = ArenaSz(arena);
  /* garbage collection ? */
  oldH = H;
  oldHB = HB;
  qd[HEAP_SIZE] = Global_MkIntegerTerm(qsz-1);
  CloseArena(oldH, oldHB, ASP, &arena, old_sz);
  tk = qd[HEAP_START];
  tv = DelBeamMin(qd+HEAP_START, qd+(HEAP_START+2*IntegerOfTerm(qd[HEAP_MAX])), qsz);
  return Yap_unify(tk, ARG2) &&
    Yap_unify(tv, ARG3);
}

#ifdef DEBUG
static Int
p_nb_beam_check(void)
{
  CELL *qd = GetHeap(ARG1,"debeam");
  UInt qsz, qmax;
  CELL *pt, *pt2;
  UInt i;

  if (!qd)
    return FALSE;
  qsz = IntegerOfTerm(qd[HEAP_SIZE]);
  qmax = IntegerOfTerm(qd[HEAP_MAX]);
  if (qsz == 0)
    return TRUE;
  pt = qd+HEAP_START;
  pt2 = pt+2*qmax;
  for (i = 1; i < qsz; i++) {
    UInt back;
    if (Yap_compare_terms(pt[2*((i+1)/2-1)],pt[2*i]) > 0) {
      Yap_DebugPlWrite(pt[2*((i+1)/2-1)]); fprintf(stderr,"\n");
      Yap_DebugPlWrite(pt[2*i]); fprintf(stderr,"\n");
      fprintf(stderr,"Error at %d\n",i);
      return FALSE;
    }
    back = IntegerOfTerm(pt[2*i+1]);
    if (IntegerOfTerm(pt2[3*back+1]) != i) {
      fprintf(stderr,"Link error at %d\n",i);
      return FALSE;
    }
  }
  for (i = 1; i < qsz; i++) {
    if (Yap_compare_terms(pt2[3*((i+1)/2-1)],pt2[3*i]) < 0) {
      fprintf(stderr,"Error at sec %d\n",i);
      Yap_DebugPlWrite(pt2[3*((i+1)/2-1)]); fprintf(stderr,"\n");
      Yap_DebugPlWrite(pt2[3*i]); fprintf(stderr,"\n");
      return FALSE;
    }
  }
  return TRUE;
}

#endif

static Int
p_nb_beam_keys(void)
{
  CELL *qd;
  UInt qsz;
  CELL *pt, *ho;
  UInt i;

 restart:
  qd = GetHeap(ARG1,"beam_keys");
  if (!qd)
    return FALSE;
  qsz = IntegerOfTerm(qd[HEAP_SIZE]);
  ho = H;
  pt = qd+HEAP_START;
  if (qsz == 0)
    return Yap_unify(ARG2, TermNil);
  for (i=0; i < qsz; i++) {
    if (H > ASP-1024) {
      H = ho;
      if (!Yap_gcl(((ASP-H)-1024)*sizeof(CELL), 2, ENV, P)) {
	Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
	return TermNil;
      }
      goto restart;
    }
    *H++ = pt[0];
    *H = AbsPair(H+1);
    H++;
    pt += 2;
  }
  H[-1] = TermNil;
  return Yap_unify(ARG2, AbsPair(ho));
}

static Int
p_nb_beam_peek(void)
{
  CELL *qd = GetHeap(ARG1,"beam_peek"), *pt, *pt2;
  UInt qsz, qbsize;
  Term tk, tv;

  if (!qd)
    return FALSE;
  qsz = IntegerOfTerm(qd[HEAP_SIZE]);
  qbsize = IntegerOfTerm(qd[HEAP_MAX]);
  if (qsz == 0)
    return FALSE;
  pt = qd+HEAP_START;
  pt2 = pt+2*qbsize;
  tk = pt[0];
  tv = pt2[2];
  return Yap_unify(tk, ARG2) &&
    Yap_unify(tv, ARG3);
}

static Int
p_nb_beam_empty(void)
{
  CELL *qd = GetHeap(ARG1,"beam_empty");

  if (!qd)
    return FALSE;
  return (IntegerOfTerm(qd[HEAP_SIZE]) == 0);
}

static Int
p_nb_beam_size(void)
{
  CELL *qd = GetHeap(ARG1,"beam_size");

  if (!qd)
    return FALSE;
  return Yap_unify(ARG2,qd[HEAP_SIZE]);
}

void Yap_InitGlobals(void)
{
  Term cm = CurrentModule;
  Yap_InitCPred("$allocate_arena", 2, p_allocate_arena, 0);
  Yap_InitCPred("$allocate_default_arena", 2, p_allocate_default_arena, 0);
  Yap_InitCPred("arena_size", 1, p_default_arena_size, 0);
  Yap_InitCPred("b_setval", 2, p_b_setval, SafePredFlag);
  Yap_InitCPred("b_getval", 2, p_nb_getval, SafePredFlag);
  Yap_InitCPred("nb_setval", 2, p_nb_setval, 0L);
  Yap_InitCPred("nb_getval", 2, p_nb_getval, SafePredFlag);
  Yap_InitCPred("nb_delete", 1, p_nb_delete, 0L);
  CurrentModule = GLOBALS_MODULE;
  Yap_InitCPred("nb_queue", 1, p_nb_queue, 0L);
  Yap_InitCPred("nb_queue_close", 3, p_nb_queue_close, SafePredFlag);
  Yap_InitCPred("nb_queue_enqueue", 2, p_nb_queue_enqueue, 0L);
  Yap_InitCPred("nb_queue_dequeue", 2, p_nb_queue_dequeue, SafePredFlag);
  Yap_InitCPred("nb_queue_peek", 2, p_nb_queue_peek, SafePredFlag);
  Yap_InitCPred("nb_queue_empty", 1, p_nb_queue_empty, SafePredFlag);
  Yap_InitCPred("nb_queue_size", 2, p_nb_queue_size, SafePredFlag);
  Yap_InitCPred("nb_heap", 2, p_nb_heap, 0L);
  Yap_InitCPred("nb_heap_close", 1, p_nb_heap_close, SafePredFlag);
  Yap_InitCPred("nb_heap_add", 3, p_nb_heap_add_to_heap, 0L);
  Yap_InitCPred("nb_heap_del", 3, p_nb_heap_del, SafePredFlag);
  Yap_InitCPred("nb_heap_peek", 3, p_nb_heap_peek, SafePredFlag);
  Yap_InitCPred("nb_heap_empty", 1, p_nb_heap_empty, SafePredFlag);
  Yap_InitCPred("nb_heap_size", 2, p_nb_heap_size, SafePredFlag);
  Yap_InitCPred("nb_beam", 2, p_nb_beam, 0L);
  Yap_InitCPred("nb_beam_close", 1, p_nb_beam_close, SafePredFlag);
  Yap_InitCPred("nb_beam_add", 3, p_nb_beam_add_to_beam, 0L);
  Yap_InitCPred("nb_beam_del", 3, p_nb_beam_del, SafePredFlag);
  Yap_InitCPred("nb_beam_peek", 3, p_nb_beam_peek, SafePredFlag);
  Yap_InitCPred("nb_beam_empty", 1, p_nb_beam_empty, SafePredFlag);
  Yap_InitCPred("nb_beam_keys", 2, p_nb_beam_keys, 0L);
#ifdef DEBUG
  Yap_InitCPred("nb_beam_check", 1, p_nb_beam_check, SafePredFlag);
#endif
  Yap_InitCPred("nb_beam_size", 2, p_nb_beam_size, SafePredFlag);
  CurrentModule = cm;
}