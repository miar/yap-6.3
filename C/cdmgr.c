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
* File:		cdmgr.c							 *
* Last rev:	8/2/88							 *
* mods:									 *
* comments:	Code manager						 *
*									 *
*************************************************************************/
#ifdef SCCS
static char     SccsId[] = "@(#)cdmgr.c	1.1 05/02/98";
#endif

#include "Yap.h"
#include "clause.h"
#include "yapio.h"
#include "eval.h"
#include "tracer.h"
#ifdef YAPOR
#include "or.macros.h"
#endif	/* YAPOR */
#if HAVE_STRING_H
#include <string.h>
#endif


STATIC_PROTO(void retract_all, (PredEntry *, int));
STATIC_PROTO(void add_first_static, (PredEntry *, yamop *, int));
STATIC_PROTO(void add_first_dynamic, (PredEntry *, yamop *, int));
STATIC_PROTO(void asserta_stat_clause, (PredEntry *, yamop *, int));
STATIC_PROTO(void asserta_dynam_clause, (PredEntry *, yamop *));
STATIC_PROTO(void assertz_stat_clause, (PredEntry *, yamop *, int));
STATIC_PROTO(void assertz_dynam_clause, (PredEntry *, yamop *));
STATIC_PROTO(void expand_consult, (void));
STATIC_PROTO(int  not_was_reconsulted, (PredEntry *, Term, int));
#if EMACS
STATIC_PROTO(int  last_clause_number, (PredEntry *));
#endif
STATIC_PROTO(int  static_in_use, (PredEntry *, int));
#if !defined(YAPOR) && !defined(THREADS)
STATIC_PROTO(Int  search_for_static_predicate_in_use, (PredEntry *, int));
STATIC_PROTO(void mark_pred, (int, PredEntry *));
STATIC_PROTO(void do_toggle_static_predicates_in_use, (int));
#endif
STATIC_PROTO(Int  p_number_of_clauses, (void));
STATIC_PROTO(Int  p_compile, (void));
STATIC_PROTO(Int  p_compile_dynamic, (void));
STATIC_PROTO(Int  p_purge_clauses, (void));
STATIC_PROTO(Int  p_setspy, (void));
STATIC_PROTO(Int  p_rmspy, (void));
STATIC_PROTO(Int  p_startconsult, (void));
STATIC_PROTO(Int  p_showconslultlev, (void));
STATIC_PROTO(Int  p_endconsult, (void));
STATIC_PROTO(Int  p_undefined, (void));
STATIC_PROTO(Int  p_in_use, (void));
STATIC_PROTO(Int  p_new_multifile, (void));
STATIC_PROTO(Int  p_is_multifile, (void));
STATIC_PROTO(Int  p_optimizer_on, (void));
STATIC_PROTO(Int  p_optimizer_off, (void));
STATIC_PROTO(Int  p_in_this_f_before, (void));
STATIC_PROTO(Int  p_first_cl_in_f, (void));
STATIC_PROTO(Int  p_mk_cl_not_first, (void));
STATIC_PROTO(Int  p_is_dynamic, (void));
STATIC_PROTO(Int  p_kill_dynamic, (void));
STATIC_PROTO(Int  p_compile_mode, (void));
STATIC_PROTO(Int  p_is_profiled, (void));
STATIC_PROTO(Int  p_profile_info, (void));
STATIC_PROTO(Int  p_profile_reset, (void));
STATIC_PROTO(Int  p_is_call_counted, (void));
STATIC_PROTO(Int  p_call_count_info, (void));
STATIC_PROTO(Int  p_call_count_set, (void));
STATIC_PROTO(Int  p_call_count_reset, (void));
STATIC_PROTO(Int  p_toggle_static_predicates_in_use, (void));
STATIC_PROTO(Atom  YapConsultingFile, (void));
STATIC_PROTO(Int  PredForCode,(yamop *, Atom *, UInt *, Term *));

#define PredArity(p) (p->ArityOfPE)
#define TRYCODE(G,F,N) ( (N)<5 ? (op_numbers)((int)F+(N)*3) : G)
#define NEXTOP(V,TYPE)    ((yamop *)(&((V)->u.TYPE.next)))

#define IN_BLOCK(P,B,SZ)     ((CODEADDR)(P) >= (CODEADDR)(B) && \
			      (CODEADDR)(P) < (CODEADDR)(B)+(SZ))

/******************************************************************
  
			EXECUTING PROLOG CLAUSES
  
******************************************************************/


static int 
static_in_use(PredEntry *p, int check_everything)
{
#if defined(YAPOR) || defined(THREADS)
  return(FALSE);
#else
  CELL pflags = p->PredFlags;
  if (pflags & (DynamicPredFlag|LogUpdatePredFlag)) {
    return (FALSE);
  }
  if (STATIC_PREDICATES_MARKED) {
    return (p->PredFlags & InUsePredFlag);
  } else {
    /* This code does not work for YAPOR or THREADS!!!!!!!! */
    return(search_for_static_predicate_in_use(p, check_everything));
  }
#endif
}

/******************************************************************
  
		ADDING AND REMOVE INFO TO A PROCEDURE
  
******************************************************************/


/*
 * we have three kinds of predicates: dynamic		DynamicPredFlag
 * static 		CompiledPredFlag fast		FastPredFlag all the
 * database predicates are supported for dynamic predicates only abolish and
 * assertz are supported for static predicates no database predicates are
 * supportted for fast predicates 
 */

#define is_dynamic(pe)  (pe->PredFlags & DynamicPredFlag)
#define is_static(pe) 	(pe->PredFlags & CompiledPredFlag)
#define is_fast(pe)	(pe->PredFlags & FastPredFlag)
#define is_logupd(pe)	(pe->PredFlags & LogUpdatePredFlag)
#ifdef TABLING
#define is_tabled(pe)   (pe->PredFlags & TabledPredFlag)
#endif /* TABLING */

/******************************************************************
  
		Indexation Info
  
******************************************************************/
#define ByteAdr(X)   ((Int) &(X))

/* Index a prolog pred, given its predicate entry */
/* ap is already locked. */
static void 
IPred(PredEntry *ap)
{
  yamop          *BaseAddr;

#ifdef DEBUG
  if (Yap_Option['i' - 'a' + 1]) {
    Term tmod = ap->ModuleOfPred;
    if (!tmod)
      tmod = TermProlog;
    Yap_DebugPutc(Yap_c_error_stream,'\t');
    Yap_plwrite(tmod, Yap_DebugPutc, 0);
    Yap_DebugPutc(Yap_c_error_stream,':');
    if (ap->ModuleOfPred == IDB_MODULE) {
      Term t = Deref(ARG1);
      if (IsAtomTerm(t)) {
	Yap_plwrite(t, Yap_DebugPutc, 0);
      } else {
	Functor f = FunctorOfTerm(t);
	Atom At = NameOfFunctor(f);
	Yap_plwrite(MkAtomTerm(At), Yap_DebugPutc, 0);
	Yap_DebugPutc(Yap_c_error_stream,'/');
	Yap_plwrite(MkIntTerm(ArityOfFunctor(f)), Yap_DebugPutc, 0);
      }
    } else {
      if (ap->ArityOfPE == 0) {
	Atom At = (Atom)ap->FunctorOfPred;
	Yap_plwrite(MkAtomTerm(At), Yap_DebugPutc, 0);
      } else {
	Functor f = ap->FunctorOfPred;
	Atom At = NameOfFunctor(f);
	Yap_plwrite(MkAtomTerm(At), Yap_DebugPutc, 0);
	Yap_DebugPutc(Yap_c_error_stream,'/');
	Yap_plwrite(MkIntTerm(ArityOfFunctor(f)), Yap_DebugPutc, 0);
      }
    }
    Yap_DebugPutc(Yap_c_error_stream,'\n');
  }
#endif
  /* Do not try to index a dynamic predicate  or one whithout args */
  if (is_dynamic(ap)) {
    Yap_Error(SYSTEM_ERROR,TermNil,"trying to index a dynamic predicate");
    return;
  }
  if ((BaseAddr = Yap_PredIsIndexable(ap)) != NULL) {
    ap->cs.p_code.TrueCodeOfPred = BaseAddr;
    ap->PredFlags |= IndexedPredFlag;
  }
  if (ap->PredFlags & SpiedPredFlag) {
    ap->OpcodeOfPred = Yap_opcode(_spy_pred);
    ap->CodeOfPred = (yamop *)(&(ap->OpcodeOfPred)); 
  } else {
    ap->CodeOfPred = ap->cs.p_code.TrueCodeOfPred;
    ap->OpcodeOfPred = ((yamop *)(ap->CodeOfPred))->opc;
  }
#ifdef DEBUG
  if (Yap_Option['i' - 'a' + 1])
    Yap_DebugPutc(Yap_c_error_stream,'\n');
#endif
}

void 
Yap_IPred(PredEntry *p)
{
  IPred(p);
}

#define GONEXT(TYPE)      code_p = ((yamop *)(&(code_p->u.TYPE.next)))

static void
RemoveMainIndex(PredEntry *ap)
{
  yamop *First = ap->cs.p_code.FirstClause;
  int spied = ap->PredFlags & SpiedPredFlag;

  ap->PredFlags &= ~IndexedPredFlag;
  if (First == NULL) {
    ap->cs.p_code.TrueCodeOfPred = FAILCODE;
  } else {
    ap->cs.p_code.TrueCodeOfPred = First;
  }
  if (First != NULL && spied) {
    ap->OpcodeOfPred = Yap_opcode(_spy_pred);
    ap->cs.p_code.TrueCodeOfPred = ap->CodeOfPred = (yamop *)(&(ap->OpcodeOfPred)); 
  } else if (ap->cs.p_code.NOfClauses > 1
#ifdef TABLING
	     ||ap->PredFlags & TabledPredFlag
#endif
	     ) {
    ap->OpcodeOfPred = INDEX_OPCODE;
    ap->CodeOfPred = ap->cs.p_code.TrueCodeOfPred = (yamop *)(&(ap->OpcodeOfPred)); 
  } else {
    ap->OpcodeOfPred = ap->cs.p_code.TrueCodeOfPred->opc;
    ap->CodeOfPred = ap->cs.p_code.TrueCodeOfPred;
  }
}

static void
decrease_ref_counter(yamop *ptr, yamop *b, yamop *e, yamop *sc)
{
  if (ptr != FAILCODE && ptr != sc && (ptr < b || ptr > e)) {
    LogUpdClause *cl = ClauseCodeToLogUpdClause(ptr);
    LOCK(cl->ClLock);
    cl->ClRefCount--;
    if (cl->ClFlags & ErasedMask &&
	!(cl->ClRefCount) &&
	!(cl->ClFlags & InUseMask)) {
      /* last ref to the clause */
      Yap_ErLogUpdCl(cl);
    }
    UNLOCK(cl->ClLock);
  }
}

static void
cleanup_dangling_indices(yamop *ipc, yamop *beg, yamop *end, yamop *suspend_code)
{
  while (ipc < end) {
    op_numbers op = Yap_op_from_opcode(ipc->opc);
    /* printf("op: %d %p->%p\n", op, ipc, end); */
    switch(op) {
    case _Ystop:
      /* end of clause, for now */
      return;
    case _index_dbref:
    case _index_blob:
      ipc = NEXTOP(ipc,e);
      break;
    case _lock_lu:
      /* just skip for now, but should worry about locking */
      ipc = NEXTOP(ipc,p);
      break;
    case _unlock_lu:
      /* just skip for now, but should worry about locking */
      ipc = NEXTOP(ipc,e);
      break;
    case _retry_profiled:
    case _count_retry:
      ipc = NEXTOP(ipc,p);
      break;
    case _retry:
    case _retry_killed:
    case _trust:
    case _trust_killed:
      decrease_ref_counter(ipc->u.ld.d, beg, end, suspend_code);
      ipc = NEXTOP(ipc,ld);
      break;
    case _try_clause:
    case _try_me:
    case _try_me1:
    case _try_me2:
    case _try_me3:
    case _try_me4:
    case _retry_me:
    case _retry_me1:
    case _retry_me2:
    case _retry_me3:
    case _retry_me4:
    case _profiled_trust_me:
    case _trust_me:
    case _count_trust_me:
    case _trust_me1:
    case _trust_me2:
    case _trust_me3:
    case _trust_me4:
      ipc = NEXTOP(ipc,ld);
      break;
    case _enter_lu_pred:
    case _stale_lu_index:
      ipc = ipc->u.Ill.l1;
      break;
    case _try_in:
    case _trust_logical_pred:
    case _jump:
    case _jump_if_var:
      ipc = NEXTOP(ipc,l);
      break;
      /* instructions type xl */
    case _jump_if_nonvar:
      ipc = NEXTOP(ipc,xl);
      break;
      /* instructions type e */
    case _switch_on_type:
      ipc = NEXTOP(ipc,llll);
      break;
    case _switch_list_nl:
      ipc = NEXTOP(ipc,ollll);
      break;
    case _switch_on_arg_type:
      ipc = NEXTOP(ipc,xllll);
      break;
    case _switch_on_sub_arg_type:
      ipc = NEXTOP(ipc,sllll);
      break;
    case _if_not_then:
      ipc = NEXTOP(ipc,clll);
      break;
    case _switch_on_func:
    case _if_func:
    case _go_on_func:
    case _switch_on_cons:
    case _if_cons:
    case _go_on_cons:
      ipc = NEXTOP(ipc,sl);
      break;
    default:
      Yap_Error(SYSTEM_ERROR,TermNil,"Bug in Indexing Code: opcode %d", op);
    }
#if defined(YAPOR) || defined(THREADS)
    ipc = (yamop *)((CELL)ipc & ~1);
#endif    
  }
}

void
Yap_cleanup_dangling_indices(yamop *ipc, yamop *beg, yamop *end, yamop *sc)
{
  cleanup_dangling_indices(ipc, beg, end, sc);
}

static void
decrease_log_indices(LogUpdIndex *c, yamop *suspend_code)
{
  /* decrease all reference counters */
  yamop *beg = c->ClCode, *end, *ipc;
  op_numbers op;
  if (c->ClFlags & SwitchTableMask) {
    return;
  }
  op = Yap_op_from_opcode(beg->opc);
  if ((op == _enter_lu_pred ||
      op == _stale_lu_index) &&
      beg->u.Ill.l1 != beg->u.Ill.l2) {
    end = beg->u.Ill.l2;
  } else {
    end = (yamop *)((CODEADDR)c+Yap_SizeOfBlock((CODEADDR)c));
  }
  ipc = beg;
  cleanup_dangling_indices(ipc, beg, end, suspend_code);
}

static void
kill_static_child_indxs(StaticIndex *indx)
{
  StaticIndex *cl = indx->ChildIndex;
  while (cl != NULL) {
    StaticIndex *next = cl->SiblingIndex;
    kill_static_child_indxs(cl);
    cl = next;
  }
  Yap_FreeCodeSpace((CODEADDR)indx);
}

static void
kill_first_log_iblock(LogUpdIndex *c, LogUpdIndex *parent, PredEntry *ap)
{
  LogUpdIndex *ncl = c->ChildIndex;

  if (parent != NULL &&
      !(c->ClFlags & ErasedMask)) {
    if (c == parent->ChildIndex) {
      parent->ChildIndex = c->SiblingIndex;
    } else {
      LogUpdIndex *tcl = parent->ChildIndex;
      while (tcl->SiblingIndex != c) {
	tcl = tcl->SiblingIndex;
      }
      tcl->SiblingIndex = c->SiblingIndex;
    }
  }
  /* make sure that a child cannot remove us */
  c->ClRefCount++;
  while (ncl != NULL) {
    LogUpdIndex *next = ncl->SiblingIndex;
    kill_first_log_iblock(ncl, c, ap);
    ncl = next;
  }
  c->ClRefCount--;
  /* check if we are still the main index */
  if (parent == NULL &&
      ap->cs.p_code.TrueCodeOfPred == c->ClCode) {
    RemoveMainIndex(ap);
  }
  if (!((c->ClFlags & InUseMask) || c->ClRefCount)) {
    if (parent != NULL) {
      parent->ClRefCount--;
      if (parent->ClFlags & ErasedMask &&
	  !(parent->ClFlags & InUseMask) &&
	  parent->ClRefCount == 0) {
	/* cool, I can erase the father too. */
	if (parent->ClFlags & SwitchRootMask) {
	  kill_first_log_iblock(parent, NULL, ap);
	} else {
	  kill_first_log_iblock(parent, parent->u.ParentIndex, ap);
	}
      }
    }
    decrease_log_indices(c, (yamop *)&(ap->cs.p_code.ExpandCode));
#ifdef DEBUG
    {
      LogUpdIndex *parent = DBErasedIList, *c0 = NULL;
      while (parent != NULL) {
	if (c == parent) {
	  if (c0) c0->SiblingIndex = c->SiblingIndex;
	  else DBErasedIList = c->SiblingIndex;
	}
	c0 = parent;
	parent = parent->SiblingIndex;
      }
    }
#endif
    Yap_FreeCodeSpace((CODEADDR)c);
  } else {
#ifdef DEBUG
    c->SiblingIndex = DBErasedIList;
    DBErasedIList = c;
#endif
    c->ClFlags |= ErasedMask;
    /* try to move up, so that we don't hold an index */
    if (parent != NULL &&
	parent->ClFlags & SwitchTableMask) {
      c->u.ParentIndex = parent->u.ParentIndex;
      parent->u.ParentIndex->ClRefCount++;
      parent->ClRefCount--;
    }
    c->ChildIndex = NULL;
  }
}

static void
kill_top_static_iblock(StaticIndex *c, PredEntry *ap)
{
  kill_static_child_indxs(c);
  RemoveMainIndex(ap);
}

void
Yap_kill_iblock(ClauseUnion *blk, ClauseUnion *parent_blk, PredEntry *ap)
{
  if (ap->PredFlags & LogUpdatePredFlag) {
    LogUpdIndex *c = (LogUpdIndex *)blk;
    if (parent_blk != NULL) {
      LogUpdIndex *cl = (LogUpdIndex *)parent_blk;
      kill_first_log_iblock(c, cl, ap);
    } else {
      kill_first_log_iblock(c, NULL, ap);
    }
  } else {
    StaticIndex *c = (StaticIndex *)blk;
    if (parent_blk != NULL) {
      StaticIndex *cl = parent_blk->si.ChildIndex;
      if (cl == c) {
	parent_blk->si.ChildIndex = c->SiblingIndex;
      } else {
	while (cl->SiblingIndex != c) {
	  cl = cl->SiblingIndex;
	}
	cl->SiblingIndex = c->SiblingIndex;
      }
    }
    kill_static_child_indxs(c);
  }
}

/*
  This predicate is supposed to be called with a
  lock on the current predicate
*/
void 
Yap_ErLogUpdIndex(LogUpdIndex *clau)
{
  LogUpdIndex *c = clau;
  if (c->ClFlags & SwitchRootMask) {
     kill_first_log_iblock(clau, NULL, c->u.pred);
 } else {
    while (!(c->ClFlags & SwitchRootMask)) 
      c = c->u.ParentIndex;
    kill_first_log_iblock(clau, clau->u.ParentIndex, c->u.pred);
 }
}

void
Yap_RemoveLogUpdIndex(LogUpdIndex *cl)
{
  if (cl->ClFlags & SwitchRootMask) {
    kill_first_log_iblock(cl, NULL, cl->u.pred);
  } else {
    LogUpdIndex *pcl = cl;
    while (!(pcl->ClFlags & SwitchRootMask)) {
      pcl = pcl->u.ParentIndex;
    }
    kill_first_log_iblock(cl, cl->u.ParentIndex, pcl->u.pred);
  }
}

/* Routine used when wanting to remove the indexation */
/* ap is known to already have been locked for WRITING */
static int 
RemoveIndexation(PredEntry *ap)
{ 
  if (ap->OpcodeOfPred == INDEX_OPCODE) {
    return TRUE;
  }
  if (ap->PredFlags & LogUpdatePredFlag) {
    kill_first_log_iblock(ClauseCodeToLogUpdIndex(ap->cs.p_code.TrueCodeOfPred), NULL, ap);
  } else {
    StaticIndex *cl;

    cl = ClauseCodeToStaticIndex(ap->cs.p_code.TrueCodeOfPred);

    kill_top_static_iblock(cl, ap);    
    
  }
  return (TRUE);
}

int 
Yap_RemoveIndexation(PredEntry *ap)
{
  return RemoveIndexation(ap);
}
/******************************************************************
  
			Adding clauses
  
******************************************************************/


#define	assertz	0
#define	consult	1
#define	asserta	2

/* p is already locked */
static void 
retract_all(PredEntry *p, int in_use)
{
  yamop          *fclause = NULL, *lclause = NULL;
  yamop          *q;

  q = p->cs.p_code.FirstClause;
  if (q != NULL) {
    if (p->PredFlags & LogUpdatePredFlag) { 
      LogUpdClause *cl = ClauseCodeToLogUpdClause(q);
      do {
	LogUpdClause *ncl = cl->ClNext;
	Yap_ErLogUpdCl(cl);
	cl = ncl;
      } while (cl != NULL);
    } else {
      StaticClause   *cl = ClauseCodeToStaticClause(q);

      do {
	if (cl->ClFlags & HasBlobsMask) {
	  DeadClause *dcl = (DeadClause *)cl;
	  dcl->NextCl = DeadClauses;
	  dcl->ClFlags = 0;
	  DeadClauses = dcl;
	} else {
	  Yap_FreeCodeSpace((char *)cl);
	}
	p->cs.p_code.NOfClauses--;
	if (cl->ClCode == p->cs.p_code.LastClause) break;
	cl = cl->ClNext;
      } while (TRUE);
    }
  }
  p->cs.p_code.FirstClause = fclause;
  p->cs.p_code.LastClause = lclause;
  if (fclause == NIL) {
    if (p->PredFlags & (DynamicPredFlag|LogUpdatePredFlag)) {
      p->OpcodeOfPred = FAIL_OPCODE;
    } else {
      p->OpcodeOfPred = UNDEF_OPCODE;
    }
    p->cs.p_code.TrueCodeOfPred = p->CodeOfPred = (yamop *)(&(p->OpcodeOfPred));
    p->StatisticsForPred.NOfEntries = 0;
    p->StatisticsForPred.NOfHeadSuccesses = 0;
    p->StatisticsForPred.NOfRetries = 0;
  } else {
    if (p->PredFlags & SpiedPredFlag) {
      p->OpcodeOfPred = Yap_opcode(_spy_pred);
      p->CodeOfPred = p->cs.p_code.TrueCodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
    } else if (p->PredFlags & IndexedPredFlag) {
      p->OpcodeOfPred = INDEX_OPCODE;
      p->CodeOfPred = p->cs.p_code.TrueCodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
    }
  }
  if (PROFILING) {
    p->PredFlags |= ProfiledPredFlag;
  } else
    p->PredFlags &= ~ProfiledPredFlag;
#ifdef YAPOR
  if (SEQUENTIAL_IS_DEFAULT) {
    p->PredFlags |= SequentialPredFlag;
  }
#endif /* YAPOR */
  Yap_PutValue(AtomAbol, MkAtomTerm(AtomTrue));
}

/* p is already locked */
static void 
add_first_static(PredEntry *p, yamop *cp, int spy_flag)
{
  yamop *pt = cp;

  if (is_logupd(p)) {
    if (p == PredGoalExpansion) {
      PRED_GOAL_EXPANSION_ON = TRUE;
      Yap_InitComma();
    }
  } else {
#ifdef YAPOR
    if (SEQUENTIAL_IS_DEFAULT) {
      p->PredFlags |= SequentialPredFlag;
      PUT_YAMOP_SEQ(pt);
    }
    if (YAMOP_LTT(pt) != 1)
      abort_optyap("YAMOP_LTT error in function add_first_static");
#endif /* YAPOR */
#ifdef TABLING
    if (is_tabled(p)) {
      p->OpcodeOfPred = INDEX_OPCODE;
      p->cs.p_code.TrueCodeOfPred = p->CodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
    }
#endif /* TABLING */
  }
  p->cs.p_code.TrueCodeOfPred = pt;
  p->cs.p_code.FirstClause = p->cs.p_code.LastClause = cp;
  p->cs.p_code.NOfClauses = 1;
  p->StatisticsForPred.NOfEntries = 0;
  p->StatisticsForPred.NOfHeadSuccesses = 0;
  p->StatisticsForPred.NOfRetries = 0;
  if (PROFILING) {
    p->PredFlags |= ProfiledPredFlag;
  } else
    p->PredFlags &= ~ProfiledPredFlag;
#ifdef YAPOR
  p->PredFlags |= SequentialPredFlag;
  PUT_YAMOP_SEQ((yamop *)cp);
#endif /* YAPOR */
  if (spy_flag) {
    p->OpcodeOfPred = Yap_opcode(_spy_pred);
    p->CodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
  }
  if ((yap_flags[SOURCE_MODE_FLAG] ||
      (p->PredFlags & MultiFileFlag)) &&
      !(p->PredFlags & (DynamicPredFlag|LogUpdatePredFlag))) {
    p->PredFlags |= SourcePredFlag;
  } else {
    p->PredFlags &= ~SourcePredFlag;
  }
}

/* p is already locked */
static void 
add_first_dynamic(PredEntry *p, yamop *cp, int spy_flag)
{
  yamop    *ncp = ((DynamicClause *)NULL)->ClCode;
  DynamicClause   *cl;
  if (p == PredGoalExpansion) {
    PRED_GOAL_EXPANSION_ON = TRUE;
    Yap_InitComma();
  }
  p->StatisticsForPred.NOfEntries = 0;
  p->StatisticsForPred.NOfHeadSuccesses = 0;
  p->StatisticsForPred.NOfRetries = 0;
  if (PROFILING) {
    p->PredFlags |= ProfiledPredFlag;
  } else
    p->PredFlags &= ~ProfiledPredFlag;
#ifdef YAPOR
  p->PredFlags |= SequentialPredFlag;
#endif /* YAPOR */
  /* allocate starter block, containing info needed to start execution,
   * that is a try_mark to start the code and a fail to finish things up */
  cl =
    (DynamicClause *) Yap_AllocCodeSpace((Int)NEXTOP(NEXTOP(NEXTOP(ncp,ld),e),e));
  if (cl == NIL) {
    Yap_Error(SYSTEM_ERROR,TermNil,"Heap crashed against Stacks");
    return;
  }
  /* skip the first entry, this contains the back link and will always be
     empty for this entry */
  ncp = (yamop *)(((CELL *)ncp)+1);
  /* next we have the flags. For this block mainly say whether we are
   *  being spied */
  cl->ClFlags = DynamicMask;
  ncp = cl->ClCode;
  INIT_LOCK(cl->ClLock);
  INIT_CLREF_COUNT(cl);
  /* next, set the first instruction to execute in the dyamic
   *  predicate */
  if (spy_flag)
    p->OpcodeOfPred = ncp->opc = Yap_opcode(_spy_or_trymark);
  else
    p->OpcodeOfPred = ncp->opc = Yap_opcode(_try_and_mark);
  ncp->u.ld.s = p->ArityOfPE;
  ncp->u.ld.p = p;
  ncp->u.ld.d = cp;
#ifdef YAPOR
  INIT_YAMOP_LTT(ncp, 1);
  PUT_YAMOP_SEQ(ncp);
#endif /* YAPOR */
  /* This is the point we enter the code */
  p->cs.p_code.TrueCodeOfPred = p->CodeOfPred = ncp;
  p->cs.p_code.NOfClauses = 1;
  /* set the first clause to have a retry and mark which will
   *  backtrack to the previous block */
  if (p->PredFlags & ProfiledPredFlag)
    cp->opc = Yap_opcode(_profiled_retry_and_mark);
  else if (p->PredFlags & CountPredFlag)
    cp->opc = Yap_opcode(_count_retry_and_mark);
  else
    cp->opc = Yap_opcode(_retry_and_mark);
  cp->u.ld.s = p->ArityOfPE;
  cp->u.ld.p = p;
  cp->u.ld.d = ncp;
  /* also, keep a backpointer for the days you delete the clause */
  ClauseCodeToDynamicClause(cp)->ClPrevious = ncp;
  /* Don't forget to say who is the only clause for the predicate so
     far */
  p->cs.p_code.LastClause = p->cs.p_code.FirstClause = cp;
  /* we're only missing what to do when we actually exit the procedure
   */
  ncp = NEXTOP(ncp,ld);
  /* and the last instruction to execute to exit the predicate, note
     the retry is pointing to this pseudo clause */
  ncp->opc = Yap_opcode(_trust_fail);
  /* we're only missing what to do when we actually exit the procedure
   */
  /* and close the code */
  ncp = NEXTOP(ncp,e);
  ncp->opc = Yap_opcode(_Ystop);
}

/* p is already locked */
static void 
asserta_stat_clause(PredEntry *p, yamop *q, int spy_flag)
{
  StaticClause *cl = ClauseCodeToStaticClause(q);

  p->cs.p_code.NOfClauses++;
  if (is_logupd(p)) {
    LogUpdClause
      *clp = ClauseCodeToLogUpdClause(p->cs.p_code.FirstClause),
      *clq = ClauseCodeToLogUpdClause(q);
    clq->ClPrev = NULL;
    clq->ClNext = clp;
    clp->ClPrev = clq;
    p->cs.p_code.FirstClause = q;
    if (p->PredFlags & SpiedPredFlag) {
      p->OpcodeOfPred = Yap_opcode(_spy_pred);
      p->cs.p_code.TrueCodeOfPred = p->CodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
    } else if (!(p->PredFlags & IndexedPredFlag)) {
      p->OpcodeOfPred = INDEX_OPCODE;
      p->cs.p_code.TrueCodeOfPred = p->CodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
    }
    return;
  }
  cl->ClNext = ClauseCodeToStaticClause(p->cs.p_code.FirstClause);
#ifdef YAPOR
  PUT_YAMOP_LTT(q, YAMOP_LTT((yamop *)(p->cs.p_code.FirstClause)) + 1);
#endif /* YAPOR */
  p->cs.p_code.FirstClause = q;
  p->cs.p_code.TrueCodeOfPred = q;
  if (p->PredFlags & SpiedPredFlag) {
    p->OpcodeOfPred = Yap_opcode(_spy_pred);
    p->CodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
  } else if (!(p->PredFlags & IndexedPredFlag)) {
    p->OpcodeOfPred = INDEX_OPCODE;
    p->CodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
  }
  p->cs.p_code.LastClause->u.ld.d = q;
}

/* p is already locked */
static void 
asserta_dynam_clause(PredEntry *p, yamop *cp)
{
  yamop        *q;
  DynamicClause *cl = ClauseCodeToDynamicClause(cp);
  q = cp;
  LOCK(ClauseCodeToDynamicClause(p->cs.p_code.FirstClause)->ClLock);
  /* also, keep backpointers for the days we'll delete all the clause */
  ClauseCodeToDynamicClause(p->cs.p_code.FirstClause)->ClPrevious = q;
  cl->ClPrevious = (yamop *)(p->CodeOfPred);
  cl->ClFlags |= DynamicMask;
  UNLOCK(ClauseCodeToDynamicClause(p->cs.p_code.FirstClause)->ClLock);
  q->u.ld.d = p->cs.p_code.FirstClause;
  q->u.ld.s = p->ArityOfPE;
  q->u.ld.p = p;
  if (p->PredFlags & ProfiledPredFlag)
    cp->opc = Yap_opcode(_profiled_retry_and_mark);
  else if (p->PredFlags & CountPredFlag)
    cp->opc = Yap_opcode(_count_retry_and_mark);
  else
    cp->opc = Yap_opcode(_retry_and_mark);
  cp->u.ld.s = p->ArityOfPE;
  cp->u.ld.p = p;
  p->cs.p_code.FirstClause = cp;
  q = p->CodeOfPred;
  q->u.ld.d = cp;
  q->u.ld.s = p->ArityOfPE;
  q->u.ld.p = p;

}

/* p is already locked */
static void 
assertz_stat_clause(PredEntry *p, yamop *cp, int spy_flag)
{
  yamop        *pt;

  p->cs.p_code.NOfClauses++;
  pt = p->cs.p_code.LastClause;
  if (is_logupd(p)) {
    LogUpdClause
      *clp = ClauseCodeToLogUpdClause(cp),
      *clq = ClauseCodeToLogUpdClause(pt);

    clq->ClNext = clp;
    clp->ClPrev = clq;
    clp->ClNext = NULL;
    p->cs.p_code.LastClause = cp;
    if (p->PredFlags & SpiedPredFlag) {
      p->OpcodeOfPred = Yap_opcode(_spy_pred);
      p->cs.p_code.TrueCodeOfPred = p->CodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
    } else if (!(p->PredFlags & IndexedPredFlag)) {
      p->OpcodeOfPred = INDEX_OPCODE;
      p->cs.p_code.TrueCodeOfPred = p->CodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
    }
    return;
  }
  if (p->cs.p_code.FirstClause == p->cs.p_code.LastClause) {
    if (!(p->PredFlags & SpiedPredFlag)) {
      p->OpcodeOfPred = INDEX_OPCODE;
      p->CodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
    }
  }
  {
      StaticClause *cl =   ClauseCodeToStaticClause(pt);
      cl->ClNext = ClauseCodeToStaticClause(cp);
  }
  p->cs.p_code.LastClause = cp;
#ifdef YAPOR
  {
    StaticClause *cl = ClauseCodeToStaticClause(p->cs.p_code.FirstClause);

    while (TRUE) {
      PUT_YAMOP_LTT((yamop *)code, YAMOP_LTT(cl->ClCode) + 1);
      if (cl->ClCode == p->cs.p_code.LastClause)
	break;
      cl = cl->NextCl;
    }
  }
#endif /* YAPOR */
}

/* p is already locked */
static void 
assertz_dynam_clause(PredEntry *p, yamop *cp)
{
  yamop       *q;
  DynamicClause *cl = ClauseCodeToDynamicClause(cp);

  q = p->cs.p_code.LastClause;
  LOCK(ClauseCodeToDynamicClause(q)->ClLock);
  q->u.ld.d = cp;
  p->cs.p_code.LastClause = cp;
  /* also, keep backpointers for the days we'll delete all the clause */
  cl->ClPrevious = q;
  cl->ClFlags |= DynamicMask;
  UNLOCK(ClauseCodeToDynamicClause(q)->ClLock);
  q = (yamop *)cp;
  if (p->PredFlags & ProfiledPredFlag)
    q->opc = Yap_opcode(_profiled_retry_and_mark);
  else if (p->PredFlags & CountPredFlag)
    q->opc = Yap_opcode(_count_retry_and_mark);
  else
    q->opc = Yap_opcode(_retry_and_mark);
  q->u.ld.d = p->CodeOfPred;
  q->u.ld.s = p->ArityOfPE;
  q->u.ld.p = p;
  p->cs.p_code.NOfClauses++;
}

static void  expand_consult(void)
{
  consult_obj *new_cl, *new_cb, *new_cs;
  UInt OldConsultCapacity = ConsultCapacity;

  /* now double consult capacity */
  ConsultCapacity += InitialConsultCapacity;
  /* I assume it always works ;-) */
  while ((new_cl = (consult_obj *)Yap_AllocCodeSpace(sizeof(consult_obj)*ConsultCapacity)) == NULL) {
    if (!Yap_growheap(FALSE, sizeof(consult_obj)*ConsultCapacity, NULL)) {
      Yap_Error(SYSTEM_ERROR,TermNil,Yap_ErrorMessage);
      return;
    }
  }
  new_cs = new_cl + (InitialConsultCapacity+1);
  new_cb = new_cs + (ConsultBase-ConsultSp);
  /* start copying */
  memcpy((void *)(new_cs), (void *)(ConsultSp), OldConsultCapacity*sizeof(consult_obj));
  /* copying done, release old space */
  Yap_FreeCodeSpace((char *)ConsultLow);
  /* next, set up pointers correctly */
  ConsultSp = new_cs;
  ConsultBase = new_cb;
  ConsultLow = new_cl;
}

/* p was already locked */
static int 
not_was_reconsulted(PredEntry *p, Term t, int mode)
{
  register consult_obj  *fp;
  Prop                   p0 = AbsProp((PropEntry *)p);

  for (fp = ConsultSp; fp < ConsultBase; ++fp)
    if (fp->p == p0)
      break;
  if (fp != ConsultBase)
    return (FALSE);
  if (mode) {
    if (ConsultSp == ConsultLow+1)
      expand_consult();
    --ConsultSp;
    ConsultSp->p = p0;
    if (ConsultBase[1].mode && 
	!(p->PredFlags & MultiFileFlag)) /* we are in reconsult mode */ {
      retract_all(p, static_in_use(p,TRUE));
    }
    p->src.OwnerFile = YapConsultingFile();
  }
  return (TRUE);		/* careful */
}

static void
addcl_permission_error(AtomEntry *ap, Int Arity, int in_use) 
{
  Term t, ti[2];

  ti[0] = MkAtomTerm(AbsAtom(ap));
  ti[1] = MkIntegerTerm(Arity);
  t = Yap_MkApplTerm(Yap_MkFunctor(Yap_LookupAtom("/"),2), 2, ti);
  Yap_ErrorMessage = Yap_ErrorSay;
  Yap_Error_Term = t;
  Yap_Error_TYPE = PERMISSION_ERROR_MODIFY_STATIC_PROCEDURE;
  if (in_use) {
    if (Arity == 0)
      sprintf(Yap_ErrorMessage, "static predicate %s is in use", ap->StrOfAE);
    else
      sprintf(Yap_ErrorMessage,
#if SHORT_INTS
	      "static predicate %s/%ld is in use",
#else
	      "static predicate %s/%d is in use",
#endif
	      ap->StrOfAE, Arity);
  } else {
    if (Arity == 0)
      sprintf(Yap_ErrorMessage, "system predicate %s", ap->StrOfAE);
    else
      sprintf(Yap_ErrorMessage,
#if SHORT_INTS
	      "system predicate %s/%ld",
#else
	      "system predicate %s/%d",
#endif
	      ap->StrOfAE, Arity);
  }
}


static Term
addclause(Term t, yamop *cp, int mode, int mod)
/*
 *
 mode
   0  assertz
   1  consult
   2  asserta
*/
{
  PredEntry      *p;
  int             spy_flag = FALSE;
  Atom           at;
  UInt           Arity;
  CELL		 pflags;
  Term		 tf;


  if (IsApplTerm(t) && FunctorOfTerm(t) == FunctorAssert)
    tf = ArgOfTerm(1, t);
  else
    tf = t;
  if (IsAtomTerm(tf)) {
    at = AtomOfTerm(tf);
    p = RepPredProp(PredPropByAtom(at, mod));
    Arity = 0;
  } else {
    Functor f = FunctorOfTerm(tf);
    Arity = ArityOfFunctor(f);
    at = NameOfFunctor(f);
    p = RepPredProp(PredPropByFunc(f, mod));
  }
  Yap_PutValue(AtomAbol, TermNil);
  WRITE_LOCK(p->PRWLock);
#if defined(YAPOR) || defined(THREADS)
  WPP = p;
#endif
  pflags = p->PredFlags;
  /* we are redefining a prolog module predicate */
  if (p->ModuleOfPred == PROLOG_MODULE && 
      mod != TermProlog && mod) {
    WRITE_UNLOCK(p->PRWLock);
    addcl_permission_error(RepAtom(at), Arity, FALSE);
    return TermNil;
  }
  /* The only problem we have now is when we need to throw away
     Indexing blocks
  */
  if (pflags & IndexedPredFlag) {
    Yap_AddClauseToIndex(p, cp, mode == asserta);
  }
  if (pflags & SpiedPredFlag)
    spy_flag = TRUE;
  if (mode == consult)
    not_was_reconsulted(p, t, TRUE);
  /* always check if we have a valid error first */
  if (Yap_ErrorMessage && Yap_Error_TYPE == PERMISSION_ERROR_MODIFY_STATIC_PROCEDURE) {
#if defined(YAPOR) || defined(THREADS)
    WPP = NULL;
#endif
    WRITE_UNLOCK(p->PRWLock);
    return TermNil;
  }
  if (!is_dynamic(p)) {
    if (pflags & LogUpdatePredFlag) {
      LogUpdClause     *clp = ClauseCodeToLogUpdClause(cp);
      clp->ClFlags |= LogUpdMask;
      if (IsAtomTerm(t) ||
	  FunctorOfTerm(t) != FunctorAssert) {
	clp->ClFlags |= FactMask;
	clp->ClSource = NULL;
      }
    } else {
      StaticClause     *clp = ClauseCodeToStaticClause(cp);
      clp->ClFlags |= StaticMask;
      if (IsAtomTerm(t) ||
	  FunctorOfTerm(t) != FunctorAssert) {
	clp->ClFlags |= FactMask;
	clp->usc.ClPred = p;
      }
    }
    if (compile_mode)
      p->PredFlags = p->PredFlags | CompiledPredFlag;
    else
      p->PredFlags = p->PredFlags | CompiledPredFlag;
  }
  if (p->cs.p_code.FirstClause == NULL) {
    if (!(pflags & DynamicPredFlag)) {
      add_first_static(p, cp, spy_flag);
      /* make sure we have a place to jump to */
      if (p->OpcodeOfPred == UNDEF_OPCODE ||
	  p->OpcodeOfPred == FAIL_OPCODE) {  /* log updates */
	p->CodeOfPred = p->cs.p_code.TrueCodeOfPred;
	p->OpcodeOfPred = ((yamop *)(p->CodeOfPred))->opc;
      }
    } else {
      add_first_dynamic(p, cp, spy_flag);
    }
  } else if (mode == asserta) {
    if (pflags & DynamicPredFlag)
      asserta_dynam_clause(p, cp);
    else
      asserta_stat_clause(p, cp, spy_flag);
  } else if (pflags & DynamicPredFlag)
    assertz_dynam_clause(p, cp);
  else {
    assertz_stat_clause(p, cp, spy_flag);
    if (p->OpcodeOfPred != INDEX_OPCODE &&
	p->OpcodeOfPred != Yap_opcode(_spy_pred)) {
      p->CodeOfPred = p->cs.p_code.TrueCodeOfPred;
      p->OpcodeOfPred = ((yamop *)(p->CodeOfPred))->opc;
    }
  }
#if defined(YAPOR) || defined(THREADS)
  WPP = NULL;
#endif
  WRITE_UNLOCK(p->PRWLock);
  if (pflags & LogUpdatePredFlag) {
    return MkDBRefTerm((DBRef)ClauseCodeToLogUpdClause(cp));
  } else {
    return MkIntegerTerm((Int)cp);
  }
}

void
Yap_addclause(Term t, yamop *cp, int mode, Term mod) {
  addclause(t, cp, mode, mod);
}

void
Yap_EraseStaticClause(StaticClause *cl, Term mod) {
  PredEntry *ap;

  /* ok, first I need to find out the parent predicate */
  if (cl->ClFlags & FactMask) {
    ap = cl->usc.ClPred;
  } else {
    Term t = ArgOfTerm(1,cl->usc.ClSource->Entry);
    if (IsAtomTerm(t)) {
      Atom at = AtomOfTerm(t);
      ap = RepPredProp(Yap_GetPredPropByAtom(at, mod));
    } else {
      Functor fun = FunctorOfTerm(t);
      ap = RepPredProp(Yap_GetPredPropByFunc(fun, mod));
    }
  }
  WRITE_LOCK(ap->PRWLock);
  if (ap->PredFlags & IndexedPredFlag)
    RemoveIndexation(ap);
  ap->cs.p_code.NOfClauses--;
  if (ap->cs.p_code.FirstClause == cl->ClCode) {
    /* got rid of first clause */
    if (ap->cs.p_code.LastClause == cl->ClCode) {
      /* got rid of all clauses */
      ap->cs.p_code.LastClause = ap->cs.p_code.FirstClause = NULL;
      ap->OpcodeOfPred = UNDEF_OPCODE;
      ap->cs.p_code.TrueCodeOfPred =
	(yamop *)(&(ap->OpcodeOfPred)); 
    } else {
      yamop *ncl = cl->ClNext->ClCode;
      ap->cs.p_code.FirstClause = ncl;
      ncl->opc = Yap_opcode(_try_me);
      ap->cs.p_code.TrueCodeOfPred =
	ncl;
      ap->OpcodeOfPred = ncl->opc;
    }
  } else {
    StaticClause *pcl = ClauseCodeToStaticClause(ap->cs.p_code.FirstClause),
      *ocl = NULL;

    while (pcl != cl) {
      ocl = pcl;
      pcl = pcl->ClNext;
    }
    ocl->ClCode->u.ld.d = cl->ClCode->u.ld.d;
    ocl->ClNext = cl->ClNext;
    if (cl->ClCode ==  ap->cs.p_code.LastClause) {
      ap->cs.p_code.LastClause = ocl->ClCode;
      if (ap->cs.p_code.NOfClauses > 1)
	ocl->ClCode->opc = Yap_opcode(_trust_me);
    }
  }
  if (ap->cs.p_code.NOfClauses == 1) {
    ap->cs.p_code.TrueCodeOfPred =
      ap->cs.p_code.FirstClause;
    ap->OpcodeOfPred =
      ap->cs.p_code.TrueCodeOfPred->opc;
  }
  WRITE_UNLOCK(ap->PRWLock);
  if (cl->ClFlags & HasBlobsMask || static_in_use(ap,TRUE)) {
    DeadClause *dcl = (DeadClause *)cl;
    dcl->NextCl = DeadClauses;
    dcl->ClFlags = 0;
    DeadClauses = dcl;
  } else {
    Yap_FreeCodeSpace((char *)cl);
  }
  if (ap->cs.p_code.NOfClauses == 0) {
    ap->CodeOfPred = 
      ap->cs.p_code.TrueCodeOfPred;
  } else if (ap->cs.p_code.NOfClauses > 1) {
    ap->OpcodeOfPred = INDEX_OPCODE;
    ap->CodeOfPred = ap->cs.p_code.TrueCodeOfPred = (yamop *)(&(ap->OpcodeOfPred)); 
  } else if (ap->PredFlags & SpiedPredFlag) {
      ap->OpcodeOfPred = Yap_opcode(_spy_pred);
      ap->CodeOfPred = ap->cs.p_code.TrueCodeOfPred = (yamop *)(&(ap->OpcodeOfPred)); 
  } else {
    ap->CodeOfPred = ap->cs.p_code.TrueCodeOfPred;
  }
}

void
Yap_add_logupd_clause(PredEntry *pe, LogUpdClause *cl, int mode) {
  yamop *cp = cl->ClCode;

  if (pe->PredFlags & IndexedPredFlag) {
    Yap_AddClauseToIndex(pe, cp, mode == asserta);
  }
  if (pe->cs.p_code.FirstClause == NULL) {
    add_first_static(pe, cp, FALSE);
    /* make sure we have a place to jump to */
    if (pe->OpcodeOfPred == UNDEF_OPCODE ||
	pe->OpcodeOfPred == FAIL_OPCODE) {  /* log updates */
      pe->CodeOfPred = pe->cs.p_code.TrueCodeOfPred;
      pe->OpcodeOfPred = ((yamop *)(pe->CodeOfPred))->opc;
    }
  } else if (mode == asserta) {
    asserta_stat_clause(pe, cp, FALSE);
  } else {
    assertz_stat_clause(pe, cp, FALSE);
    if (pe->OpcodeOfPred != INDEX_OPCODE &&
	pe->OpcodeOfPred != Yap_opcode(_spy_pred)) {
      pe->CodeOfPred = pe->cs.p_code.TrueCodeOfPred;
      pe->OpcodeOfPred = ((yamop *)(pe->CodeOfPred))->opc;
    }
  }
}

static Int 
p_in_this_f_before(void)
{				/* '$in_this_file_before'(N,A,M) */
  unsigned int    arity;
  Atom            at;
  Term            t;
  register consult_obj  *fp;
  Prop            p0;
  Term            mod;

  if (IsVarTerm(t = Deref(ARG1)) || !IsAtomTerm(t))
    return (FALSE);
  else
    at = AtomOfTerm(t);
  if (IsVarTerm(t = Deref(ARG2)) || !IsIntTerm(t))
    return (FALSE);
  else
    arity = IntOfTerm(t);
  if (IsVarTerm(mod = Deref(ARG3)) || !IsAtomTerm(mod))
    return FALSE;
  if (arity)
    p0 = PredPropByFunc(Yap_MkFunctor(at, arity),CurrentModule);
  else
    p0 = PredPropByAtom(at, CurrentModule);
  if (ConsultSp == ConsultBase || (fp = ConsultSp)->p == p0)
    return (FALSE);
  else
    fp++;
  for (; fp < ConsultBase; ++fp)
    if (fp->p == p0)
      break;
  if (fp != ConsultBase)
    return (TRUE);
  else
    return (FALSE);
}

static Int 
p_first_cl_in_f(void)
{				/* '$first_cl_in_file'(+N,+Ar,+Mod) */
  unsigned int    arity;
  Atom            at;
  Term            t;
  register consult_obj  *fp;
  Prop            p0;
  Term	          mod;
  

  if (IsVarTerm(t = Deref(ARG1)) || !IsAtomTerm(t))
    return (FALSE);
  else
    at = AtomOfTerm(t);
  if (IsVarTerm(t = Deref(ARG2)) || !IsIntTerm(t))
    return (FALSE);
  else
    arity = IntOfTerm(t);
  if (IsVarTerm(mod = Deref(ARG3)) || !IsAtomTerm(mod))
    return (FALSE);
  if (arity)
    p0 = PredPropByFunc(Yap_MkFunctor(at, arity),mod);
  else
    p0 = PredPropByAtom(at, mod);
  for (fp = ConsultSp; fp < ConsultBase; ++fp)
    if (fp->p == p0)
      break;
  if (fp != ConsultBase)
    return (FALSE);
  return (TRUE);
}

static Int 
p_mk_cl_not_first(void)
{				/* '$mk_cl_not_first'(+N,+Ar) */
  unsigned int    arity;
  Atom            at;
  Term            t;
  Prop            p0;

  if (IsVarTerm(t = Deref(ARG1)) && !IsAtomTerm(t))
    return (FALSE);
  else
    at = AtomOfTerm(t);
  if (IsVarTerm(t = Deref(ARG2)) && !IsIntTerm(t))
    return (FALSE);
  else
    arity = IntOfTerm(t);
  if (arity)
    p0 = PredPropByFunc(Yap_MkFunctor(at, arity),CurrentModule);
  else
    p0 = PredPropByAtom(at, CurrentModule);
  --ConsultSp;
  ConsultSp->p = p0;
  return (TRUE);
}

#if EMACS

/*
 * the place where one would add a new clause for the propriety pred_prop 
 */
int 
where_new_clause(pred_prop, mode)
     Prop            pred_prop;
     int             mode;
{
  PredEntry      *p = RepPredProp(pred_prop);

  if (mode == consult && not_was_reconsulted(p, TermNil, FALSE))
    return (1);
  else
    return (p->cs.p_code.NOfClauses + 1);
}
#endif

static Int 
p_compile(void)
{				/* '$compile'(+C,+Flags, Mod) */
  Term            t = Deref(ARG1);
  Term            t1 = Deref(ARG2);
  Term            mod = Deref(ARG4);
  yamop           *codeadr;

  if (IsVarTerm(t1) || !IsIntTerm(t1))
    return (FALSE);
  if (IsVarTerm(mod) || !IsAtomTerm(mod))
    return (FALSE);

  YAPEnterCriticalSection();
  codeadr = Yap_cclause(t, 2, mod, Deref(ARG3)); /* vsc: give the number of arguments
			      to cclause in case there is overflow */
  t = Deref(ARG1);        /* just in case there was an heap overflow */
  if (!Yap_ErrorMessage)
    addclause(t, codeadr, (int) (IntOfTerm(t1) & 3), mod);
  YAPLeaveCriticalSection();
  if (Yap_ErrorMessage) {
    if (IntOfTerm(t1) & 4) {
      Yap_Error(Yap_Error_TYPE, Yap_Error_Term,
	    "in line %d, %s", Yap_FirstLineInParse(), Yap_ErrorMessage);
    } else
      Yap_Error(Yap_Error_TYPE, Yap_Error_Term, Yap_ErrorMessage);
    return (FALSE);
  }
  return (TRUE);
}

static Int 
p_compile_dynamic(void)
{				/* '$compile_dynamic'(+C,+Flags,Mod,-Ref) */
  Term            t = Deref(ARG1);
  Term            t1 = Deref(ARG2);
  Term            mod = Deref(ARG4);
  yamop        *code_adr;
  int             old_optimize;

  if (IsVarTerm(t1) || !IsIntTerm(t1))
    return (FALSE);
  if (IsVarTerm(mod) || !IsAtomTerm(mod))
    return (FALSE);
  old_optimize = optimizer_on;
  optimizer_on = FALSE;
  YAPEnterCriticalSection();
  code_adr = Yap_cclause(t, 3, mod, Deref(ARG3)); /* vsc: give the number of arguments to
			       cclause() in case there is a overflow */
  t = Deref(ARG1);        /* just in case there was an heap overflow */
  if (!Yap_ErrorMessage) {
    
    optimizer_on = old_optimize;
    t = addclause(t, code_adr, (int) (IntOfTerm(t1) & 3), mod);
  } else {
    if (IntOfTerm(t1) & 4) {
      Yap_Error(Yap_Error_TYPE, Yap_Error_Term, "line %d, %s", Yap_FirstLineInParse(), Yap_ErrorMessage);
    } else
      Yap_Error(Yap_Error_TYPE, Yap_Error_Term, Yap_ErrorMessage);
    YAPLeaveCriticalSection();
    return (FALSE);
  }
  YAPLeaveCriticalSection();
  return Yap_unify(ARG5, t);
}

static int      consult_level = 0;

static Atom
YapConsultingFile (void)
{
  if (consult_level == 0) {
    return(Yap_LookupAtom("user"));
  } else {
    return(Yap_LookupAtom(ConsultBase[2].filename));
  }
}

Atom
Yap_ConsultingFile (void)
{
  return YapConsultingFile();
}

/* consult file *file*, *mode* may be one of either consult or reconsult */
static void
init_consult(int mode, char *file)
{
  ConsultSp--;
  ConsultSp->filename = file;
  ConsultSp--;
  ConsultSp->mode = mode;
  ConsultSp--;
  ConsultSp->c = (ConsultBase-ConsultSp);
  ConsultBase = ConsultSp;
#if !defined(YAPOR) && !defined(SBA)
  /*  if (consult_level == 0)
      do_toggle_static_predicates_in_use(TRUE); */
#endif
  consult_level++;
}

void
Yap_init_consult(int mode, char *file)
{
  init_consult(mode,file);
}

static Int 
p_startconsult(void)
{				/* '$start_consult'(+Mode)	 */
  Term            t;
  char           *smode = RepAtom(AtomOfTerm(Deref(ARG1)))->StrOfAE;
  int             mode;
  
  mode = strcmp("consult",smode);
  init_consult(mode, RepAtom(AtomOfTerm(Deref(ARG2)))->StrOfAE);
  t = MkIntTerm(consult_level);
  return (Yap_unify_constant(ARG3, t));
}

static Int 
p_showconslultlev(void)
{
  Term            t;

  t = MkIntTerm(consult_level);
  return (Yap_unify_constant(ARG1, t));
}

static void
end_consult(void)
{
  ConsultSp = ConsultBase;
  ConsultBase = ConsultSp+ConsultSp->c;
  ConsultSp += 3;
  consult_level--;
#if !defined(YAPOR) && !defined(SBA)
  /*  if (consult_level == 0)
      do_toggle_static_predicates_in_use(FALSE);*/
#endif
}

void
Yap_end_consult(void) {
  end_consult();
}


static Int 
p_endconsult(void)
{				/* '$end_consult'		 */
  end_consult();
  return (TRUE);
}

static void
purge_clauses(PredEntry *pred)
{
  yamop          *q;
  int		  in_use;

  if (pred->PredFlags & IndexedPredFlag)
    RemoveIndexation(pred);
  Yap_PutValue(AtomAbol, MkAtomTerm(AtomTrue));
  q = pred->cs.p_code.FirstClause;
  in_use = static_in_use(pred,TRUE);
  if (q != NULL) {
    if (pred->PredFlags & LogUpdatePredFlag) {
      LogUpdClause *cl = ClauseCodeToLogUpdClause(q);
      do {
	LogUpdClause *ncl = cl->ClNext;
	Yap_ErLogUpdCl(cl);
	cl = ncl;
      } while (cl != NULL);
    } else {
      StaticClause *cl = ClauseCodeToStaticClause(q);

      do {
	if (cl->ClFlags & HasBlobsMask || in_use) {
	  DeadClause *dcl = (DeadClause *)cl;
	  dcl->NextCl = DeadClauses;
	  dcl->ClFlags = 0;
	  DeadClauses = dcl;
	} else {
	  Yap_FreeCodeSpace((char *)cl);
	}
	if (cl->ClCode == pred->cs.p_code.LastClause) break;
	cl = cl->ClNext;
      } while (TRUE);
    }
  }
  pred->cs.p_code.FirstClause = pred->cs.p_code.LastClause = NULL;
  if (pred->PredFlags & (DynamicPredFlag|LogUpdatePredFlag)) {
    pred->OpcodeOfPred = FAIL_OPCODE;
  } else {
    pred->OpcodeOfPred = UNDEF_OPCODE;
  }
  pred->cs.p_code.TrueCodeOfPred =
    pred->CodeOfPred =
    (yamop *)(&(pred->OpcodeOfPred)); 
  pred->src.OwnerFile = AtomNil;
  if (pred->PredFlags & MultiFileFlag)
    pred->PredFlags ^= MultiFileFlag;
}

void
Yap_Abolish(PredEntry *pred)
{
  purge_clauses(pred);
}

static Int 
p_purge_clauses(void)
{				/* '$purge_clauses'(+Func) */
  PredEntry      *pred;
  Term            t = Deref(ARG1);
  Term            mod = Deref(ARG2);

  Yap_PutValue(AtomAbol, MkAtomTerm(AtomNil));
  if (IsVarTerm(t))
    return FALSE;
  if (IsVarTerm(mod) || !IsAtomTerm(mod)) {
    return FALSE;
  }
  if (IsAtomTerm(t)) {
    Atom at = AtomOfTerm(t);
    pred = RepPredProp(PredPropByAtom(at, mod));
  } else if (IsApplTerm(t)) {
    Functor         fun = FunctorOfTerm(t);
    pred = RepPredProp(PredPropByFunc(fun, mod));
  } else
    return (FALSE);
  WRITE_LOCK(pred->PRWLock);
#if defined(YAPOR) || defined(THREADS)
  WPP = pred;
#endif
  if (pred->PredFlags & StandardPredFlag) {
#if defined(YAPOR) || defined(THREADS)
    WPP = NULL;
#endif
    WRITE_UNLOCK(pred->PRWLock);
    Yap_Error(PERMISSION_ERROR_MODIFY_STATIC_PROCEDURE, t, "assert/1");
    return (FALSE);
  }
  purge_clauses(pred);
#if defined(YAPOR) || defined(THREADS)
  WPP = NULL;
#endif
  WRITE_UNLOCK(pred->PRWLock);
  return (TRUE);
}

/******************************************************************
  
		MANAGING SPY-POINTS
  
******************************************************************/

static Int 
p_setspy(void)
{				/* '$set_spy'(+Fun,+M)	 */
  Atom            at;
  PredEntry      *pred;
  CELL            fg;
  Term            t, mod;

  at = Yap_FullLookupAtom("$spy");
  pred = RepPredProp(PredPropByFunc(Yap_MkFunctor(at, 1),0));
  SpyCode = pred;
  t = Deref(ARG1);
  mod = Deref(ARG2);
  if (IsVarTerm(mod) || !IsAtomTerm(mod))
    return (FALSE);
  if (IsVarTerm(t))
    return (FALSE);
  if (IsAtomTerm(t)) {
    Atom at = AtomOfTerm(t);
    pred = RepPredProp(Yap_PredPropByAtomNonThreadLocal(at, mod));
  } else if (IsApplTerm(t)) {
    Functor fun = FunctorOfTerm(t);
    pred = RepPredProp(Yap_PredPropByFunctorNonThreadLocal(fun, mod));
  } else {
    return (FALSE);
  }
  WRITE_LOCK(pred->PRWLock);
 restart_spy:
  if (pred->PredFlags & (CPredFlag | SafePredFlag)) {
    WRITE_UNLOCK(pred->PRWLock);
    return (FALSE);
  }
  if (pred->OpcodeOfPred == UNDEF_OPCODE ||
      pred->OpcodeOfPred == FAIL_OPCODE) {
    WRITE_UNLOCK(pred->PRWLock);
    return (FALSE);
  }
  if (pred->OpcodeOfPred == INDEX_OPCODE) {
    IPred(pred);
    goto restart_spy;
  }
  fg = pred->PredFlags;
  if (fg & DynamicPredFlag) {
    pred->OpcodeOfPred =
      ((yamop *)(pred->CodeOfPred))->opc =
      Yap_opcode(_spy_or_trymark);
  } else {
    pred->OpcodeOfPred = Yap_opcode(_spy_pred);
    pred->CodeOfPred = (yamop *)(&(pred->OpcodeOfPred)); 
  }
  pred->PredFlags |= SpiedPredFlag;
  WRITE_UNLOCK(pred->PRWLock);
  return (TRUE);
}

static Int 
p_rmspy(void)
{				/* '$rm_spy'(+T,+Mod)	 */
  Atom            at;
  PredEntry      *pred;
  Term            t;
  Term            mod;

  t = Deref(ARG1);
  mod = Deref(ARG2);
  if (IsVarTerm(mod) || !IsAtomTerm(mod))
    return (FALSE);
  if (IsVarTerm(t))
    return (FALSE);
  if (IsAtomTerm(t)) {
    at = AtomOfTerm(t);
    pred = RepPredProp(Yap_PredPropByAtomNonThreadLocal(at, mod));
  } else if (IsApplTerm(t)) {
    Functor fun = FunctorOfTerm(t);
    pred = RepPredProp(Yap_PredPropByFunctorNonThreadLocal(fun, mod));
  } else
    return (FALSE);
  WRITE_LOCK(pred->PRWLock);
  if (!(pred->PredFlags & SpiedPredFlag)) {
    WRITE_UNLOCK(pred->PRWLock);
    return (FALSE);
  }
#if THREADS
  if (!(pred->PredFlags & ThreadLocalPredFlag)) {
    pred->OpcodeOfPred = Yap_opcode(_thread_local);
  } else
#endif
    if (!(pred->PredFlags & DynamicPredFlag)) {
    pred->CodeOfPred = pred->cs.p_code.TrueCodeOfPred;
    pred->OpcodeOfPred = ((yamop *)(pred->CodeOfPred))->opc;
  } else if (pred->OpcodeOfPred == Yap_opcode(_spy_or_trymark)) {
    pred->OpcodeOfPred = Yap_opcode(_try_and_mark);
  } else
    return (FALSE);
  pred->PredFlags ^= SpiedPredFlag;
  WRITE_UNLOCK(pred->PRWLock);
  return (TRUE);
}


/******************************************************************
  
		INFO ABOUT PREDICATES
  
******************************************************************/

static Int 
p_number_of_clauses(void)
{				/* '$number_of_clauses'(Predicate,M,N) */
  Term            t = Deref(ARG1);
  Term            mod = Deref(ARG2);
  int ncl = 0;
  Prop            pe;

  if (IsVarTerm(mod)  || !IsAtomTerm(mod)) {
    return(FALSE);
  }
  if (IsAtomTerm(t)) {
    Atom a = AtomOfTerm(t);
    pe = Yap_GetPredPropByAtom(a, mod);
  } else if (IsApplTerm(t)) {
    register Functor f = FunctorOfTerm(t);
    pe = Yap_GetPredPropByFunc(f, mod);
  } else {
    return (FALSE);
  }
  if (EndOfPAEntr(pe))
    return FALSE;
  READ_LOCK(RepPredProp(pe)->PRWLock);
  ncl = RepPredProp(pe)->cs.p_code.NOfClauses;
  READ_UNLOCK(RepPredProp(pe)->PRWLock);
  return (Yap_unify_constant(ARG3, MkIntegerTerm(ncl)));
}

static Int 
p_in_use(void)
{				/* '$in_use'(+P,+Mod)	 */
  Term            t = Deref(ARG1);
  Term            mod = Deref(ARG2);
  PredEntry      *pe;
  Int            out;

  if (IsVarTerm(t))
    return (FALSE);
  if (IsVarTerm(mod) || !IsAtomTerm(mod))
    return (FALSE);
  if (IsAtomTerm(t)) {
    Atom at = AtomOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByAtom(at, mod));
  } else if (IsApplTerm(t)) {
    Functor         fun = FunctorOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByFunc(fun, mod));
  } else
    return FALSE;
  if (EndOfPAEntr(pe))
    return FALSE;
  READ_LOCK(pe->PRWLock);
  out = static_in_use(pe,TRUE);
  READ_UNLOCK(pe->PRWLock);
  return(out);
}

static Int 
p_new_multifile(void)
{				/* '$new_multifile'(+N,+Ar,+Mod)  */
  Atom            at;
  int             arity;
  PredEntry      *pe;
  Term            t = Deref(ARG1);
  Term            mod = Deref(ARG3);

  if (IsVarTerm(t))
    return (FALSE);
  if (IsAtomTerm(t))
    at = AtomOfTerm(t);
  else
    return (FALSE);
  t = Deref(ARG2);
  if (IsVarTerm(t))
    return (FALSE);
  if (IsIntTerm(t))
    arity = IntOfTerm(t);
  else
    return (FALSE);
  if (arity == 0) 
    pe = RepPredProp(PredPropByAtom(at, mod));
  else 
    pe = RepPredProp(PredPropByFunc(Yap_MkFunctor(at, arity),mod));
  WRITE_LOCK(pe->PRWLock);
  pe->PredFlags |= MultiFileFlag;
  if (!(pe->PredFlags & (DynamicPredFlag|LogUpdatePredFlag))) {
    /* static */
    pe->PredFlags |= (SourcePredFlag|CompiledPredFlag);
  }
  WRITE_UNLOCK(pe->PRWLock);
  return (TRUE);
}


static Int 
p_is_multifile(void)
{				/* '$is_multifile'(+S,+Mod)	 */
  PredEntry      *pe;
  Term            t = Deref(ARG1);
  Term            mod = Deref(ARG2);
  Int		  out;

  if (IsVarTerm(t))
    return (FALSE);
  if (IsVarTerm(mod))
    return (FALSE);
  if (!IsAtomTerm(mod))
    return (FALSE);
  if (IsAtomTerm(t)) {
    pe = RepPredProp(Yap_GetPredPropByAtom(AtomOfTerm(t), mod));
  } else if (IsApplTerm(t)) {
    pe = RepPredProp(Yap_GetPredPropByFunc(FunctorOfTerm(t), mod));
  } else
    return(FALSE);
  if (EndOfPAEntr(pe))
    return FALSE;
  READ_LOCK(pe->PRWLock);
  out = (pe->PredFlags & MultiFileFlag);
  READ_UNLOCK(pe->PRWLock);
  return(out);
}

static Int 
p_is_log_updatable(void)
{				/* '$is_dynamic'(+P)	 */
  PredEntry      *pe;
  Term            t = Deref(ARG1);
  Int             out;
  Term            mod = Deref(ARG2);

  if (IsVarTerm(t)) {
    return (FALSE);
  } else if (IsAtomTerm(t)) {
    Atom at = AtomOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByAtom(at, mod));
  } else if (IsApplTerm(t)) {
    Functor         fun = FunctorOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByFunc(fun, mod));
  } else
    return (FALSE);
  if (EndOfPAEntr(pe))
    return FALSE;
  READ_LOCK(pe->PRWLock);
  out = (pe->PredFlags & LogUpdatePredFlag);
  READ_UNLOCK(pe->PRWLock);
  return(out);
}

static Int 
p_is_source(void)
{				/* '$is_dynamic'(+P)	 */
  PredEntry      *pe;
  Term            t = Deref(ARG1);
  Term            mod = Deref(ARG2);
  Int             out;

  if (IsVarTerm(t)) {
    return (FALSE);
  } else if (IsAtomTerm(t)) {
    Atom at = AtomOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByAtom(at, mod));
  } else if (IsApplTerm(t)) {
    Functor         fun = FunctorOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByFunc(fun, mod));
  } else
    return (FALSE);
  if (EndOfPAEntr(pe))
    return FALSE;
  READ_LOCK(pe->PRWLock);
  out = (pe->PredFlags & SourcePredFlag);
  READ_UNLOCK(pe->PRWLock);
  return(out);
}

static Int 
p_is_dynamic(void)
{				/* '$is_dynamic'(+P)	 */
  PredEntry      *pe;
  Term            t = Deref(ARG1);
  Term            mod = Deref(ARG2);
  Int             out;

  if (IsVarTerm(t)) {
    return (FALSE);
  } else if (IsAtomTerm(t)) {
    Atom at = AtomOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByAtom(at, mod));
  } else if (IsApplTerm(t)) {
    Functor         fun = FunctorOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByFunc(fun, mod));
  } else
    return (FALSE);
  if (EndOfPAEntr(pe))
    return FALSE;
  READ_LOCK(pe->PRWLock);
  out = (pe->PredFlags & (DynamicPredFlag|LogUpdatePredFlag));
  READ_UNLOCK(pe->PRWLock);
  return(out);
}

static Int 
p_pred_exists(void)
{				/* '$pred_exists'(+P,+M)	 */
  PredEntry      *pe;
  Term            t = Deref(ARG1);
  Term            mod = Deref(ARG2);
  Int             out;

  if (IsVarTerm(t)) {
    return (FALSE);
  } else if (IsAtomTerm(t)) {
    Atom at = AtomOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByAtom(at, mod));
  } else if (IsApplTerm(t)) {
    Functor         fun = FunctorOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByFunc(fun, mod));
  } else
    return (FALSE);
  if (EndOfPAEntr(pe))
    return FALSE;
  READ_LOCK(pe->PRWLock);
  if (pe->PredFlags & HiddenPredFlag)
    return(FALSE);
  out = (pe->OpcodeOfPred != UNDEF_OPCODE);
  READ_UNLOCK(pe->PRWLock);
  return(out);
}

static Int 
p_set_pred_module(void)
{				/* '$set_pred_module'(+P,+Mod)	 */
  PredEntry      *pe;
  Term            t = Deref(ARG1);
  Term            mod = CurrentModule;

 restart_set_pred:
  if (IsVarTerm(t)) {
    return (FALSE);
  } else if (IsAtomTerm(t)) {
    pe = RepPredProp(PredPropByAtom(AtomOfTerm(t), mod));
  } else if (IsApplTerm(t)) {
    Functor         fun = FunctorOfTerm(t);
    if (fun == FunctorModule) {
      Term tmod = ArgOfTerm(1, t);
      if (IsVarTerm(tmod) ) {
	Yap_Error(INSTANTIATION_ERROR,ARG1,"set_pred_module/1");
	return(FALSE);
      }
      if (!IsAtomTerm(tmod) ) {
	Yap_Error(TYPE_ERROR_ATOM,ARG1,"set_pred_module/1");
	return(FALSE);
      }
      mod = tmod;
      t = ArgOfTerm(2, t);
      goto restart_set_pred;
    }
    pe = RepPredProp(PredPropByFunc(fun, mod));
  } else
    return (FALSE);
  if (EndOfPAEntr(pe))
    return FALSE;
  WRITE_LOCK(pe->PRWLock);
  pe->ModuleOfPred = Deref(ARG2);
  WRITE_UNLOCK(pe->PRWLock);
  return(TRUE);
}

static Int 
p_undefined(void)
{				/* '$undefined'(P,Mod)	 */
  PredEntry      *pe;
  Term            t;
  Term            mod;

  t = Deref(ARG1);
  mod = Deref(ARG2);
  if (IsVarTerm(mod)) {
    Yap_Error(INSTANTIATION_ERROR,ARG2,"undefined/1");
    return(FALSE);
  }
  if (!IsAtomTerm(mod)) {
    Yap_Error(TYPE_ERROR_ATOM,ARG2,"undefined/1");
    return(FALSE);
  }
 restart_undefined:
  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,ARG1,"undefined/1");
    return(FALSE);
  }
  if (IsAtomTerm(t)) {
    Atom at = AtomOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByAtom(at,mod));
  } else if (IsApplTerm(t)) {
    Functor         funt = FunctorOfTerm(t);
    if (funt == FunctorModule) {
      Term tmod = ArgOfTerm(1, t);
      if (IsVarTerm(tmod) ) {
	Yap_Error(INSTANTIATION_ERROR,ARG1,"undefined/1");
	return(FALSE);
      }
      if (!IsAtomTerm(tmod) ) {
	Yap_Error(TYPE_ERROR_ATOM,ARG1,"undefined/1");
	return(FALSE);
      }
      mod = tmod;
      t = ArgOfTerm(2, t);
      goto restart_undefined;
    }
    pe = RepPredProp(Yap_GetPredPropByFunc(funt, mod));
  } else {
    return TRUE;
  }
  if (EndOfPAEntr(pe))
    return TRUE;
  READ_LOCK(pe->PRWLock);
  if (pe->PredFlags & (CPredFlag|UserCPredFlag|TestPredFlag|AsmPredFlag|DynamicPredFlag|LogUpdatePredFlag)) {
    READ_UNLOCK(pe->PRWLock);
    return FALSE;
  }
  if (pe->OpcodeOfPred == UNDEF_OPCODE) {
    READ_UNLOCK(pe->PRWLock);
    return TRUE;
  }
  READ_UNLOCK(pe->PRWLock);
  return FALSE;
}

/*
 * this predicate should only be called when all clauses for the dynamic
 * predicate were remove, otherwise chaos will follow!! 
 */

static Int 
p_kill_dynamic(void)
{				/* '$kill_dynamic'(P,M)       */
  PredEntry      *pe;
  Term            t;
  Term            mod;

  mod = Deref(ARG2);
  if (IsVarTerm(mod)) {
    Yap_Error(INSTANTIATION_ERROR,ARG2,"undefined/1");
    return(FALSE);
  }
  if (!IsAtomTerm(mod)) {
    Yap_Error(TYPE_ERROR_ATOM,ARG2,"undefined/1");
    return(FALSE);
  }
  t = Deref(ARG1);
  if (IsAtomTerm(t)) {
    Atom at = AtomOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByAtom(at, mod));
  } else if (IsApplTerm(t)) {
    Functor         funt = FunctorOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByFunc(funt, mod));
  } else
    return (FALSE);
  if (EndOfPAEntr(pe))
    return TRUE;
  WRITE_LOCK(pe->PRWLock);
  if (!(pe->PredFlags & (DynamicPredFlag|LogUpdatePredFlag))) {
    WRITE_UNLOCK(pe->PRWLock);
    return (FALSE);
  }
  if (pe->cs.p_code.LastClause != pe->cs.p_code.FirstClause) {
    WRITE_UNLOCK(pe->PRWLock);
    return (FALSE);
  }
  pe->cs.p_code.LastClause = pe->cs.p_code.FirstClause = NIL;
  pe->OpcodeOfPred = UNDEF_OPCODE;
  pe->cs.p_code.TrueCodeOfPred = pe->CodeOfPred = (yamop *)(&(pe->OpcodeOfPred)); 
  pe->PredFlags = 0L;
  WRITE_UNLOCK(pe->PRWLock);
  return (TRUE);
}

static Int 
p_optimizer_on(void)
{				/* '$optimizer_on'		 */
  optimizer_on = TRUE;
  return (TRUE);
}

static Int 
p_optimizer_off(void)
{				/* '$optimizer_off'		 */
  optimizer_on = FALSE;
  return (TRUE);
}

static Int 
p_compile_mode(void)
{				/* $compile_mode(Old,New)	 */
  Term            t2, t3 = MkIntTerm(compile_mode);
  if (!Yap_unify_constant(ARG1, t3))
    return (FALSE);
  t2 = Deref(ARG2);
  if (IsVarTerm(t2) || !IsIntTerm(t2))
    return (FALSE);
  compile_mode = IntOfTerm(t2) & 1;
  return (TRUE);
}

#if !defined(YAPOR)
static yamop *cur_clause(PredEntry *pe, yamop *codeptr)
{
  StaticClause *cl;

  cl = ClauseCodeToStaticClause(pe->cs.p_code.FirstClause);
  do {
    if (IN_BLOCK(codeptr,cl,Yap_SizeOfBlock((CODEADDR)cl))) {
      return cl->ClCode;
    }
    if (cl->ClCode == pe->cs.p_code.LastClause)
      break;
    cl = cl->ClNext;
  } while (TRUE);
  Yap_Error(SYSTEM_ERROR,TermNil,"could not find clause for indexing code");
  return(NULL);
}

static yamop *cur_log_upd_clause(PredEntry *pe, yamop *codeptr)
{
  LogUpdClause *cl;
  cl = ClauseCodeToLogUpdClause(pe->cs.p_code.FirstClause);
  do {
    if (IN_BLOCK(codeptr,cl->ClCode,Yap_SizeOfBlock((CODEADDR)cl))) {
      return((yamop *)cl->ClCode);
    }
    cl = cl->ClNext;
  } while (cl != NULL);
  Yap_Error(SYSTEM_ERROR,TermNil,"could not find clause for indexing code");
  return(NULL);
}

static LogUpdIndex *
find_owner_log_index(LogUpdIndex *cl, yamop *code_p)
{
  yamop *code_beg = cl->ClCode;
  yamop *code_end = (yamop *)((char *)cl + Yap_SizeOfBlock((CODEADDR)cl));
  
  if (code_p >= code_beg && code_p <= code_end) {
    return cl;
  }
  cl = cl->ChildIndex;
  while (cl != NULL) {
    LogUpdIndex *out;
    if ((out = find_owner_log_index(cl, code_p)) != NULL) {
      return out;
    }
    cl = cl->SiblingIndex;
  }
  return NULL;
}

static StaticIndex *
find_owner_static_index(StaticIndex *cl, yamop *code_p)
{
  yamop *code_beg = cl->ClCode;
  yamop *code_end = (yamop *)((char *)cl + Yap_SizeOfBlock((CODEADDR)cl));
  
  if (code_p >= code_beg && code_p <= code_end) {
    return cl;
  }
  cl = cl->ChildIndex;
  while (cl != NULL) {
    StaticIndex *out;
    if ((out = find_owner_static_index(cl, code_p)) != NULL) {
      return out;
    }
    cl = cl->SiblingIndex;
  }
  return NULL;
}

ClauseUnion *
Yap_find_owner_index(yamop *ipc, PredEntry *ap)
{
  /* we assume we have an owner index */
  if (ap->PredFlags & LogUpdatePredFlag) {
    LogUpdIndex *cl = ClauseCodeToLogUpdIndex(ap->cs.p_code.TrueCodeOfPred);
    return (ClauseUnion *)find_owner_log_index(cl,ipc);
  } else {
    StaticIndex *cl = ClauseCodeToStaticIndex(ap->cs.p_code.TrueCodeOfPred);
    return (ClauseUnion *)find_owner_static_index(cl,ipc);
  }
}

static Int
search_for_static_predicate_in_use(PredEntry *p, int check_everything)
{
  choiceptr b_ptr = B;
  CELL *env_ptr = ENV;

  if (check_everything) {
    PredEntry *pe = EnvPreg(P);
    if (p == pe) return(TRUE);
    pe = EnvPreg(CP);
    if (p == pe) return(TRUE);
  }
  do {
    /* check first environments that are younger than our latest choicepoint */
    if (check_everything) {
      /* 
	 I do not need to check environments for asserts,
	 only for retracts
      */
      while (b_ptr > (choiceptr)env_ptr) {
	PredEntry *pe = EnvPreg(env_ptr[E_CP]);
	if (p == pe) return(TRUE);
	if (env_ptr != NULL)
	  env_ptr = (CELL *)(env_ptr[E_E]);
      }
    }
    /* now mark the choicepoint */
    if (b_ptr != NULL) {
      PredEntry *pe;
      op_numbers opnum = Yap_op_from_opcode(b_ptr->cp_ap->opc);
      
    restart_cp:
      switch(opnum) {
      case _or_else:
      case _or_last:
	if (!check_everything) {
	  b_ptr = b_ptr->cp_b;
	  continue;
	}
#ifdef YAPOR
	pe = b_ptr->cp_cp->u.ldl.p;
#else
	pe = b_ptr->cp_cp->u.sla.p0;
#endif /* YAPOR */
	break;
      case _retry_profiled:
	opnum = Yap_op_from_opcode(NEXTOP(b_ptr->cp_ap,l)->opc);
	goto restart_cp;
      case _count_retry:
	opnum = Yap_op_from_opcode(NEXTOP(b_ptr->cp_ap,l)->opc);
	goto restart_cp;
      default:
	pe = (PredEntry *)(b_ptr->cp_ap->u.ld.p);
      }
      if (pe == p) {
	if (check_everything)
	  return TRUE;
	READ_LOCK(pe->PRWLock);
	if (p->PredFlags & IndexedPredFlag) {
	  yamop *code_p = b_ptr->cp_ap;
	  yamop *code_beg = p->cs.p_code.TrueCodeOfPred;

	  if (p->PredFlags & LogUpdatePredFlag) {
	    LogUpdIndex *cl = ClauseCodeToLogUpdIndex(code_beg);
	    if (find_owner_log_index(cl, code_p)) 
	      b_ptr->cp_ap = cur_log_upd_clause(pe, b_ptr->cp_ap->u.ld.d);
	  } else {
	    /* static clause */
	    StaticIndex *cl = ClauseCodeToStaticIndex(code_beg);
	    if (find_owner_static_index(cl, code_p)) {
	      b_ptr->cp_ap = cur_clause(pe, b_ptr->cp_ap->u.ld.d);
	    }
	  }
	}
      }
      READ_UNLOCK(pe->PRWLock);
      env_ptr = b_ptr->cp_env;
      b_ptr = b_ptr->cp_b;
    }
  } while (b_ptr != NULL);
  return(FALSE);
}

static void
mark_pred(int mark, PredEntry *pe)
{
  /* if the predicate is static mark it */
  if (pe->ModuleOfPred) {
    WRITE_LOCK(pe->PRWLock);
    if (mark) {
      pe->PredFlags |= InUsePredFlag;
    } else {
      pe->PredFlags &= ~InUsePredFlag;
    }
    WRITE_UNLOCK(pe->PRWLock);
  }
}

/* go up the chain of choice_points and environments,
   marking all static predicates that current execution is depending 
   upon */
static void
do_toggle_static_predicates_in_use(int mask)
{
  choiceptr b_ptr = B;
  CELL *env_ptr = ENV;

  if (b_ptr == NULL)
    return;

  do {
    PredEntry *pe;
    /* check first environments that are younger than our latest choicepoint */
    while (b_ptr > (choiceptr)env_ptr) {
      PredEntry *pe = EnvPreg(env_ptr[E_CP]);
      
      mark_pred(mask, pe);
      env_ptr = (CELL *)(env_ptr[E_E]);
    }
    /* now mark the choicepoint */
    {
      op_numbers opnum;
    restart_cp:
      opnum = Yap_op_from_opcode(b_ptr->cp_ap->opc);
      
      switch(opnum) {
      case _or_else:
      case _or_last:
#ifdef YAPOR
	pe = b_ptr->cp_cp->u.ldl.p;
#else
	pe = b_ptr->cp_cp->u.sla.p0;
#endif /* YAPOR */
	break;
      case _Nstop:
	pe = NULL;
	break;
      case _retry_profiled:
	opnum = Yap_op_from_opcode(NEXTOP(b_ptr->cp_ap,l)->opc);
	goto restart_cp;
      case _count_retry:
	opnum = Yap_op_from_opcode(NEXTOP(b_ptr->cp_ap,l)->opc);
	goto restart_cp;
      default:
	pe = (PredEntry *)(b_ptr->cp_ap->u.ld.p);
      }
      if (pe != NULL)
	mark_pred(mask, pe);
      env_ptr = b_ptr->cp_env;
      b_ptr = b_ptr->cp_b;
    }
  } while (b_ptr != NULL);
  /* mark or unmark all predicates */
  STATIC_PREDICATES_MARKED = mask;
}

#endif

static Term
all_envs(CELL *env_ptr)
{
  Term tf = AbsPair(H);
  CELL *bp = NULL;
  
  /* walk the environment chain */
  while (env_ptr != NULL) {
    bp = H;
    H += 2;
    /* notice that MkIntegerTerm may increase the Heap */
    bp[0] = MkIntegerTerm((Int)env_ptr[E_CP]);
    if (H >= ASP) {
      bp[1] = TermNil;
      return tf;
    } else {
      bp[1] = AbsPair(H);
    }
    env_ptr = (CELL *)(env_ptr[E_E]);      
  }
  bp[1] = TermNil;
  return tf;
}

static Term
all_cps(choiceptr b_ptr)
{
  CELL *bp = NULL;
  Term tf = AbsPair(H);

  while (b_ptr != NULL) {
    bp = H;
    H += 2;
    /* notice that MkIntegerTerm may increase the Heap */
    bp[0] = MkIntegerTerm((Int)b_ptr->cp_ap);
    if (H >= ASP) {
      bp[1] = TermNil;
      return tf;
    } else {
      bp[1] = AbsPair(H);
    }
    b_ptr = b_ptr->cp_b;
  }
  bp[1] = TermNil;
  return tf;
}


static Term
all_calls(void)
{
  Term ts[3];
  Functor f = Yap_MkFunctor(AtomLocal,3);

  ts[0] = MkIntegerTerm((Int)P);
  if (yap_flags[STACK_DUMP_ON_ERROR_FLAG]) {
    ts[1] = all_envs(ENV);
    ts[2] = all_cps(B);
  } else {
    ts[1] = ts[2] = TermNil;
  }
  return(Yap_MkApplTerm(f,3,ts));
}

Term
Yap_all_calls(void)
{
  return all_calls();
}

static Int
p_current_stack(void)
{
#ifdef YAPOR
  return(FALSE);
#else
  return(Yap_unify(ARG1,all_calls()));
#endif
}

/* This predicate is to be used by reconsult to mark all predicates
   currently in use as being executed.

   The idea is to go up the chain of choice_points and environments.

 */
static Int
p_toggle_static_predicates_in_use(void)
{
#if !defined(YAPOR) && !defined(THREADS)
  Term t = Deref(ARG1);
  Int mask;
  
  /* find out whether we need to mark or unmark */
  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"toggle_static_predicates_in_use/1");
    return(FALSE);
  }
  if (!IsIntTerm(t)) {
    Yap_Error(TYPE_ERROR_INTEGER,t,"toggle_static_predicates_in_use/1");
    return(FALSE);
  }  else {
    mask = IntOfTerm(t);
  }
  do_toggle_static_predicates_in_use(mask);
#endif
  return(TRUE);
}

static void
clause_was_found(PredEntry *pp, Atom *pat, UInt *parity) {
  /* we found it */
  *parity = pp->ArityOfPE;
  if (pp->ArityOfPE) {
    *pat = NameOfFunctor(pp->FunctorOfPred);
  } else {
    *pat = (Atom)(pp->FunctorOfPred);
  }
}

static void
code_in_pred_info(PredEntry *pp, Atom *pat, UInt *parity) {
  *parity = pp->ArityOfPE;
  if (pp->ArityOfPE) {
    *pat = NameOfFunctor(pp->FunctorOfPred);
  } else {
    *pat = (Atom)(pp->FunctorOfPred);
  }  
}

static int
code_in_pred_lu_index(LogUpdIndex *icl, yamop *codeptr) {
  LogUpdIndex *cicl;
  if (IN_BLOCK(codeptr,icl,Yap_SizeOfBlock((CODEADDR)icl))) {
    return TRUE;
  }
  cicl = icl->ChildIndex;
  while (cicl != NULL) {
    if (code_in_pred_lu_index(cicl, codeptr))
      return TRUE;
    cicl = cicl->SiblingIndex;
  }
  return FALSE;
}

static int
code_in_pred_s_index(StaticIndex *icl, yamop *codeptr) {
  StaticIndex *cicl;
  if (IN_BLOCK(codeptr,icl,Yap_SizeOfBlock((CODEADDR)icl))) {
    return TRUE;
  }
  cicl = icl->ChildIndex;
  while (cicl != NULL) {
    if (code_in_pred_s_index(cicl, codeptr))
      return TRUE;
    cicl = cicl->SiblingIndex;
  }
  return FALSE;
}

static Int
code_in_pred(PredEntry *pp, Atom *pat, UInt *parity, yamop *codeptr) {
  yamop *clcode;
  int i = 1;

  READ_LOCK(pp->PRWLock);
  /* check if the codeptr comes from the indexing code */
  if (pp->PredFlags & IndexedPredFlag) {
    if (pp->PredFlags & LogUpdatePredFlag) {
      if (code_in_pred_lu_index(ClauseCodeToLogUpdIndex(pp->cs.p_code.TrueCodeOfPred), codeptr)) {
	code_in_pred_info(pp, pat, parity);
	READ_UNLOCK(pp->PRWLock);
	return -1;
      }
    } else {
      if (code_in_pred_s_index(ClauseCodeToStaticIndex(pp->cs.p_code.TrueCodeOfPred), codeptr)) {
	code_in_pred_info(pp, pat, parity);
	READ_UNLOCK(pp->PRWLock);
	return -1;
      }
    }
  }
  clcode = pp->cs.p_code.FirstClause;
  if (clcode != NULL) {
    char *code_end;
    if (pp->PredFlags & LogUpdatePredFlag) {
      LogUpdClause *cl = ClauseCodeToLogUpdClause(pp->cs.p_code.TrueCodeOfPred);
      code_end = (char *)cl + Yap_SizeOfBlock((CODEADDR)cl);
    } else if (!(pp->PredFlags & DynamicPredFlag)) {
      code_end = NULL;
    } else {
      StaticClause *cl = ClauseCodeToStaticClause(pp->cs.p_code.TrueCodeOfPred);
      code_end = (char *)cl + Yap_SizeOfBlock((CODEADDR)cl);
    }
    if (pp->PredFlags & LogUpdatePredFlag) {
      LogUpdClause *cl = ClauseCodeToLogUpdClause(clcode);
      do {
	if (IN_BLOCK(codeptr,(CODEADDR)cl,Yap_SizeOfBlock((CODEADDR)cl))) {
	  clause_was_found(pp, pat, parity);
	  READ_UNLOCK(pp->PRWLock);
	  return i;
	}
	i++;
	cl = cl->ClNext;
      } while (cl != NULL);
    } else if (pp->PredFlags & DynamicPredFlag) {
      do {
	CODEADDR cl;
	
	cl = (CODEADDR)ClauseCodeToDynamicClause(clcode);
	if (IN_BLOCK(codeptr,cl,Yap_SizeOfBlock((CODEADDR)cl))) {
	  clause_was_found(pp, pat, parity);
	  READ_UNLOCK(pp->PRWLock);
	  return i;
	}
	if (clcode == pp->cs.p_code.LastClause)
	  break;
	i++;
	clcode = NextDynamicClause(clcode);
      } while (TRUE);
    } else {
      StaticClause *cl;
	
      cl = ClauseCodeToStaticClause(clcode);
      do {
	if (IN_BLOCK(codeptr,cl,Yap_SizeOfBlock((CODEADDR)cl))) {
	  clause_was_found(pp, pat, parity);
	  READ_UNLOCK(pp->PRWLock);
	  return i;
	}
	if (cl->ClCode == pp->cs.p_code.LastClause)
	  break;
	i++;
	cl = cl->ClNext;
      } while (TRUE);
    }
  }
  READ_UNLOCK(pp->PRWLock); 
  return(0);
}

static Int
PredForCode(yamop *codeptr, Atom *pat, UInt *parity, Term *pmodule) {
  Int found = 0;
  Int i_table;

  /* should we allow the user to see hidden predicates? */
  for (i_table = NoOfModules-1; i_table >= 0; --i_table) {
    PredEntry *pp = ModulePred[i_table];
    while (pp != NULL) {
      if ((found = code_in_pred(pp,  pat, parity, codeptr)) != 0) {
	*pmodule = i_table;
	return(found);
      }
      pp = pp->NextPredOfModule;
    }
  }
  return(0);
}

Int
Yap_PredForCode(yamop *codeptr, Atom *pat, UInt *parity, Term *pmodule) {
  return PredForCode(codeptr, pat, parity, pmodule);
}


static Int
p_pred_for_code(void) {
  yamop *codeptr;
  Atom at;
  UInt arity;
  Term module;
  Int cl;
  Term t = Deref(ARG1);

  if (IsVarTerm(t)) {
    return FALSE;
  } else if (IsIntegerTerm(t)) {
    codeptr  = (yamop *)IntegerOfTerm(t);
  } else if (IsDBRefTerm(t)) {
    codeptr  = (yamop *)DBRefOfTerm(t);
  } else {
    return FALSE;
  }
  cl = PredForCode(codeptr, &at, &arity, &module);
  if (!module) module = TermProlog;
  if (cl == 0) {
    return(Yap_unify(ARG5,MkIntTerm(0)));
  } else {
    return(Yap_unify(ARG2,MkAtomTerm(at)) &&
	   Yap_unify(ARG3,MkIntegerTerm(arity)) &&
	   Yap_unify(ARG4,module) &&
	   Yap_unify(ARG5,MkIntegerTerm(cl)));
  }
}

static Int
p_is_profiled(void)
{
  Term t = Deref(ARG1);
  char *s;

  if (IsVarTerm(t)) {
    Term ta;

    if (PROFILING) ta = MkAtomTerm(Yap_LookupAtom("on"));
    else ta = MkAtomTerm(Yap_LookupAtom("off"));
    BIND((CELL *)t,ta,bind_is_profiled);
#ifdef COROUTINING
    DO_TRAIL(CellPtr(t), ta);
    if (CellPtr(t) < H0) Yap_WakeUp((CELL *)t);
  bind_is_profiled:
#endif
    return(TRUE);
  } else if (!IsAtomTerm(t)) return(FALSE);
  s = RepAtom(AtomOfTerm(t))->StrOfAE;
  if (strcmp(s,"on") == 0) {
    PROFILING = TRUE;
    Yap_InitComma();
    return(TRUE);
  } else if (strcmp(s,"off") == 0) {
    PROFILING = FALSE;
    Yap_InitComma();
    return(TRUE);
  }
  return(FALSE);
}

static Int
p_profile_info(void)
{
  Term mod = Deref(ARG1);
  Term tfun = Deref(ARG2);
  Term out;
  PredEntry *pe;
  Term p[3];

  if (IsVarTerm(mod) || !IsAtomTerm(mod))
    return(FALSE);
  if (IsVarTerm(tfun)) {
    return(FALSE);
  } else if (IsApplTerm(tfun)) {
    Functor f = FunctorOfTerm(tfun);
    if (IsExtensionFunctor(f)) {
      return(FALSE);
    }
    pe = RepPredProp(Yap_GetPredPropByFunc(f, mod));
  } else if (IsAtomTerm(tfun)) {
    pe = RepPredProp(Yap_GetPredPropByAtom(AtomOfTerm(tfun), mod));
  } else {
    return(FALSE);
  }
  if (EndOfPAEntr(pe))
    return(FALSE);
  LOCK(pe->StatisticsForPred.lock);
  if (!(pe->StatisticsForPred.NOfEntries)) {
    UNLOCK(pe->StatisticsForPred.lock);
    return(FALSE);
  }
  p[0] = Yap_MkULLIntTerm(pe->StatisticsForPred.NOfEntries);
  p[1] = Yap_MkULLIntTerm(pe->StatisticsForPred.NOfHeadSuccesses);
  p[2] = Yap_MkULLIntTerm(pe->StatisticsForPred.NOfRetries);
  UNLOCK(pe->StatisticsForPred.lock);
  out = Yap_MkApplTerm(Yap_MkFunctor(AtomProfile,3),3,p);
  return(Yap_unify(ARG3,out));
}

static Int
p_profile_reset(void)
{
  Term mod = Deref(ARG1);
  Term tfun = Deref(ARG2);
  PredEntry *pe;

  if (IsVarTerm(mod) || !IsAtomTerm(mod))
    return(FALSE);
  if (IsVarTerm(tfun)) {
    return(FALSE);
  } else if (IsApplTerm(tfun)) {
    Functor f = FunctorOfTerm(tfun);
    if (IsExtensionFunctor(f)) {
      return(FALSE);
    }
    pe = RepPredProp(Yap_GetPredPropByFunc(f, mod));
  } else if (IsAtomTerm(tfun)) {
    pe = RepPredProp(Yap_GetPredPropByAtom(AtomOfTerm(tfun), mod));
  } else {
    return(FALSE);
  }
  if (EndOfPAEntr(pe))
    return(FALSE);
  LOCK(pe->StatisticsForPred.lock);
  pe->StatisticsForPred.NOfEntries = 0;
  pe->StatisticsForPred.NOfHeadSuccesses = 0;
  pe->StatisticsForPred.NOfRetries = 0;
  UNLOCK(pe->StatisticsForPred.lock);
  return(TRUE);
}

static Int
p_is_call_counted(void)
{
  Term t = Deref(ARG1);
  char *s;

  if (IsVarTerm(t)) {
    Term ta;

    if (CALL_COUNTING) ta = MkAtomTerm(Yap_LookupAtom("on"));
    else ta = MkAtomTerm(Yap_LookupAtom("off"));
    BIND((CELL *)t,ta,bind_is_call_counted);
#ifdef COROUTINING
    DO_TRAIL(CellPtr(t), ta);
    if (CellPtr(t) < H0) Yap_WakeUp((CELL *)t);
  bind_is_call_counted:
#endif
    return(TRUE);
  } else if (!IsAtomTerm(t)) return(FALSE);
  s = RepAtom(AtomOfTerm(t))->StrOfAE;
  if (strcmp(s,"on") == 0) {
    CALL_COUNTING = TRUE;
    Yap_InitComma();
    return(TRUE);
  } else if (strcmp(s,"off") == 0) {
    CALL_COUNTING = FALSE;
    Yap_InitComma();
    return(TRUE);
  }
  return(FALSE);
}

static Int
p_call_count_info(void)
{
  return(Yap_unify(MkIntegerTerm(ReductionsCounter),ARG1) &&
	 Yap_unify(MkIntegerTerm(PredEntriesCounter),ARG2) &&
	 Yap_unify(MkIntegerTerm(PredEntriesCounter),ARG3));
}

static Int
p_call_count_reset(void)
{
  ReductionsCounter = 0;
  ReductionsCounterOn = FALSE;
  PredEntriesCounter = 0;
  PredEntriesCounterOn = FALSE;
  RetriesCounter = 0;
  RetriesCounterOn = FALSE;
  return(TRUE);
}

static Int
p_call_count_set(void)
{
  int do_calls = IntOfTerm(ARG2);
  int do_retries = IntOfTerm(ARG4);
  int do_entries = IntOfTerm(ARG6);

  if (do_calls)
    ReductionsCounter = IntegerOfTerm(Deref(ARG1));
  ReductionsCounterOn = do_calls;
  if (do_retries)
    RetriesCounter = IntegerOfTerm(Deref(ARG3));
  RetriesCounterOn = do_retries;
  if (do_entries)
    PredEntriesCounter = IntegerOfTerm(Deref(ARG5));
  PredEntriesCounterOn = do_entries;
  return(TRUE);
}

static Int
p_clean_up_dead_clauses(void)
{
  while (DeadClauses != NULL) {
    char *pt = (char *)DeadClauses;
    DeadClauses = DeadClauses->NextCl;
    Yap_FreeCodeSpace(pt);
  }
  return(TRUE);
}

static Int			/* $parent_pred(Module, Name, Arity) */
p_parent_pred(void)
{
  /* This predicate is called from the debugger.
     We assume a sequence of the form a -> b */
  Atom at;
  UInt arity;
  Term module;
  if (!PredForCode(P_before_spy, &at, &arity, &module)) {
    return(Yap_unify(ARG1, MkIntTerm(0)) &&
	   Yap_unify(ARG2, MkAtomTerm(AtomMetaCall)) &&
	   Yap_unify(ARG3, MkIntTerm(0)));
  }
  return(Yap_unify(ARG1, MkIntTerm(module)) &&
	 Yap_unify(ARG2, MkAtomTerm(at)) &&
	 Yap_unify(ARG3, MkIntTerm(arity)));
}

static Int			/* $system_predicate(P) */
p_system_pred(void)
{
  PredEntry      *pe;

  Term t1 = Deref(ARG1);
  Term mod = Deref(ARG2);

 restart_system_pred:
  if (IsVarTerm(t1))
    return (FALSE);
  if (IsAtomTerm(t1)) {
    pe = RepPredProp(Yap_GetPredPropByAtom(AtomOfTerm(t1), mod));
  } else if (IsApplTerm(t1)) {
    Functor         funt = FunctorOfTerm(t1);
    if (IsExtensionFunctor(funt)) {
      return(FALSE);
    } 
    if (funt == FunctorModule) {
      Term nmod = ArgOfTerm(1, t1);
      if (IsVarTerm(nmod)) {
	Yap_Error(INSTANTIATION_ERROR,ARG1,"system_predicate/1");
	return(FALSE);
      } 
      if (!IsAtomTerm(nmod)) {
	Yap_Error(TYPE_ERROR_ATOM,ARG1,"system_predicate/1");
	return(FALSE);
      }
      t1 = ArgOfTerm(2, t1);
      goto restart_system_pred;
    }
    pe = RepPredProp(Yap_GetPredPropByFunc(funt, mod));
  } else if (IsPairTerm(t1)) {
    return (TRUE);
  } else
    return (FALSE);
  if (EndOfPAEntr(pe))
    return(FALSE);
  return(!pe->ModuleOfPred || pe->PredFlags & (UserCPredFlag|CPredFlag|BinaryTestPredFlag|AsmPredFlag|TestPredFlag));
}

static Int			/* $system_predicate(P) */
p_hide_predicate(void)
{
  PredEntry      *pe;

  Term t1 = Deref(ARG1);
  Term mod = Deref(ARG2);

 restart_system_pred:
  if (IsVarTerm(t1))
    return (FALSE);
  if (IsAtomTerm(t1)) {
    pe = RepPredProp(Yap_GetPredPropByAtom(AtomOfTerm(t1), mod));
  } else if (IsApplTerm(t1)) {
    Functor         funt = FunctorOfTerm(t1);
    if (IsExtensionFunctor(funt)) {
      return(FALSE);
    } 
    if (funt == FunctorModule) {
      Term nmod = ArgOfTerm(1, t1);
      if (IsVarTerm(nmod)) {
	Yap_Error(INSTANTIATION_ERROR,ARG1,"hide_predicate/1");
	return(FALSE);
      } 
      if (!IsAtomTerm(nmod)) {
	Yap_Error(TYPE_ERROR_ATOM,ARG1,"hide_predicate/1");
	return(FALSE);
      }
      t1 = ArgOfTerm(2, t1);
      goto restart_system_pred;
    }
    pe = RepPredProp(Yap_GetPredPropByFunc(funt, mod));
  } else if (IsPairTerm(t1)) {
    return (TRUE);
  } else
    return (FALSE);
  if (EndOfPAEntr(pe))
    return(FALSE);
  pe->PredFlags |= HiddenPredFlag;
  return(TRUE);
}

static Int			/* $hidden_predicate(P) */
p_hidden_predicate(void)
{
  PredEntry      *pe;

  Term t1 = Deref(ARG1);
  Term mod = Deref(ARG2);

 restart_system_pred:
  if (IsVarTerm(t1))
    return (FALSE);
  if (IsAtomTerm(t1)) {
    pe = RepPredProp(Yap_GetPredPropByAtom(AtomOfTerm(t1), mod));
  } else if (IsApplTerm(t1)) {
    Functor         funt = FunctorOfTerm(t1);
    if (IsExtensionFunctor(funt)) {
      return(FALSE);
    } 
    if (funt == FunctorModule) {
      Term nmod = ArgOfTerm(1, t1);
      if (IsVarTerm(nmod)) {
	Yap_Error(INSTANTIATION_ERROR,ARG1,"hide_predicate/1");
	return(FALSE);
      } 
      if (!IsAtomTerm(nmod)) {
	Yap_Error(TYPE_ERROR_ATOM,ARG1,"hide_predicate/1");
	return(FALSE);
      }
      t1 = ArgOfTerm(2, t1);
      goto restart_system_pred;
    }
    pe = RepPredProp(Yap_GetPredPropByFunc(funt, mod));
  } else if (IsPairTerm(t1)) {
    return (TRUE);
  } else
    return (FALSE);
  if (EndOfPAEntr(pe))
    return(FALSE);
  return(pe->PredFlags & HiddenPredFlag);
}

static PredEntry *
get_pred(Term t1, Term mod, char *command)
{

 restart_system_pred:
  if (IsVarTerm(t1))
    return NULL;
  if (IsAtomTerm(t1)) {
    return RepPredProp(Yap_GetPredPropByAtom(AtomOfTerm(t1), mod));
  } else if (IsApplTerm(t1)) {
    Functor         funt = FunctorOfTerm(t1);
    if (IsExtensionFunctor(funt)) {
      return NULL;
    } 
    if (funt == FunctorModule) {
      Term nmod = ArgOfTerm(1, t1);
      if (IsVarTerm(nmod)) {
	Yap_Error(INSTANTIATION_ERROR,t1,command);
	return NULL;
      } 
      if (!IsAtomTerm(nmod)) {
	Yap_Error(TYPE_ERROR_ATOM,t1,command);
	return NULL;
      }
      t1 = ArgOfTerm(2, t1);
      goto restart_system_pred;
    }
    return RepPredProp(Yap_GetPredPropByFunc(funt, mod));
  } else if (IsPairTerm(t1)) {
    return NULL;
  } else
    return NULL;
}

static Int
fetch_next_lu_clause(PredEntry *pe, yamop *i_code, Term th, Term tb, Term tr, yamop *cp_ptr, int first_time)
{
  LogUpdClause *cl;
  Term rtn;

  cl = Yap_FollowIndexingCode(pe, i_code, th, tb, tr, NEXTOP(PredLogUpdClause->CodeOfPred,ld), cp_ptr);
  if (cl == NULL) {
#if defined(YAPOR) || defined(THREADS)
    WPP = NULL;
#endif
    WRITE_UNLOCK(pe->PRWLock);
    return FALSE;
  }
  rtn = MkDBRefTerm((DBRef)cl);
#if defined(YAPOR) || defined(THREADS)
  LOCK(cl->ClLock);
  TRAIL_CLREF(cl);		/* So that fail will erase it */
  INC_CLREF_COUNT(cl);
  UNLOCK(cl->ClLock);
#else
  if (!(cl->ClFlags & InUseMask)) {
    cl->ClFlags |= InUseMask;
    TRAIL_CLREF(cl);	/* So that fail will erase it */
  }
#endif
#if defined(YAPOR) || defined(THREADS)
  WPP = NULL;
#endif
  WRITE_UNLOCK(pe->PRWLock);
  if (cl->ClFlags & FactMask) {
    if (!Yap_unify(tb, MkAtomTerm(AtomTrue)) ||
	!Yap_unify(tr, rtn))
      return FALSE;
    if (pe->ArityOfPE) {
      Functor f = FunctorOfTerm(th);
      UInt arity = ArityOfFunctor(f), i;
      CELL *pt = RepAppl(th)+1;

      for (i=0; i<arity; i++) {
	XREGS[i+1] = pt[i];
      }
      /* don't need no ENV */
      if (first_time) {
	CP = P;
	ENV = YENV;
	YENV = ASP;
	YENV[E_CB] = (CELL) B;
      }
      READ_LOCK(pe->PRWLock);
      P = cl->ClCode;
      READ_UNLOCK(pe->PRWLock);
    }
    return TRUE;
  } else {
    Term t;

    while ((t = Yap_FetchTermFromDB(cl->ClSource)) == 0L) {
      if (first_time) {
	if (!Yap_gc(4, YENV, P)) {
	  Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
	  return FALSE;
	}
      } else {
	if (!Yap_gc(5, ENV, CP)) {
	  Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
	  return FALSE;
	}
      }
    }
    return(Yap_unify(th, ArgOfTerm(1,t)) &&
	   Yap_unify(tb, ArgOfTerm(2,t)) &&
	   Yap_unify(tr, rtn));
  }
}

static Int			/* $hidden_predicate(P) */
p_log_update_clause(void)
{
  PredEntry      *pe;
  Term t1 = Deref(ARG1);

  pe = get_pred(t1, Deref(ARG2), "clause/3");
  if (pe == NULL || EndOfPAEntr(pe))
    return FALSE;
  WRITE_LOCK(pe->PRWLock);
#if defined(YAPOR) || defined(THREADS)
  WPP = pe;
#endif
  if(pe->OpcodeOfPred == INDEX_OPCODE) {
    IPred(pe);
  }
  return fetch_next_lu_clause(pe, pe->cs.p_code.TrueCodeOfPred, t1, ARG3, ARG4, P, TRUE);
}

static Int			/* $hidden_predicate(P) */
p_continue_log_update_clause(void)
{
  PredEntry *pe = (PredEntry *)IntegerOfTerm(Deref(ARG1));
  yamop *ipc = (yamop *)IntegerOfTerm(ARG2);

  WRITE_LOCK(pe->PRWLock);
#if defined(YAPOR) || defined(THREADS)
  WPP = pe;
#endif
  return fetch_next_lu_clause(pe, ipc, Deref(ARG3), ARG4, ARG5, B->cp_ap, FALSE);
}

static Int
fetch_next_lu_clause0(PredEntry *pe, yamop *i_code, Term th, Term tb, yamop *cp_ptr, int first_time)
{
  LogUpdClause *cl;

  cl = Yap_FollowIndexingCode(pe, i_code, th, tb, TermNil, NEXTOP(PredLogUpdClause0->CodeOfPred,ld), cp_ptr);
#if defined(YAPOR) || defined(THREADS)
  WPP = NULL;
#endif
  WRITE_UNLOCK(pe->PRWLock);
  if (cl == NULL) {
    return FALSE;
  }
  if (cl->ClFlags & FactMask) {
    if (!Yap_unify(tb, MkAtomTerm(AtomTrue)))
      return FALSE;
    if (pe->ArityOfPE) {
      Functor f = FunctorOfTerm(th);
      UInt arity = ArityOfFunctor(f), i;
      CELL *pt = RepAppl(th)+1;

      for (i=0; i<arity; i++) {
	XREGS[i+1] = pt[i];
      }
      /* don't need no ENV */
      if (first_time) {
	CP = P;
	ENV = YENV;
	YENV = ASP;
	YENV[E_CB] = (CELL) B;
      }
      READ_LOCK(pe->PRWLock);
      P = cl->ClCode;
      READ_UNLOCK(pe->PRWLock);
    }
    return TRUE;
  } else {
    Term t;

    while ((t = Yap_FetchTermFromDB(cl->ClSource)) == 0L) {
      if (first_time) {
	if (!Yap_gc(4, YENV, P)) {
	  Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
	  return FALSE;
	}
      } else {
	if (!Yap_gc(5, ENV, CP)) {
	  Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
	  return FALSE;
	}
      }
    }
    return(Yap_unify(th, ArgOfTerm(1,t)) &&
	   Yap_unify(tb, ArgOfTerm(2,t)));
  }
}

static Int			/* $hidden_predicate(P) */
p_log_update_clause0(void)
{
  PredEntry      *pe;
  Term           t1 = Deref(ARG1);

  pe = get_pred(t1, Deref(ARG2), "clause/3");
  if (pe == NULL || EndOfPAEntr(pe))
    return FALSE;
  WRITE_LOCK(pe->PRWLock);
#if defined(YAPOR) || defined(THREADS)
  WPP = pe;
#endif
  if(pe->OpcodeOfPred == INDEX_OPCODE) {
    IPred(pe);
  }
  return fetch_next_lu_clause0(pe, pe->cs.p_code.TrueCodeOfPred, t1, ARG3, P, TRUE);
}

static Int			/* $hidden_predicate(P) */
p_continue_log_update_clause0(void)
{
  PredEntry *pe = (PredEntry *)IntegerOfTerm(Deref(ARG1));
  yamop *ipc = (yamop *)IntegerOfTerm(ARG2);

  WRITE_LOCK(pe->PRWLock);
#if defined(YAPOR) || defined(THREADS)
  WPP = pe;
#endif
  return fetch_next_lu_clause0(pe, ipc, Deref(ARG3), ARG4, B->cp_ap, FALSE);
}

static Int
fetch_next_static_clause(PredEntry *pe, yamop *i_code, Term th, Term tb, Term tr, yamop *cp_ptr, int first_time)
{
  StaticClause *cl;
  Term rtn;

  cl = (StaticClause *)Yap_FollowIndexingCode(pe, i_code, th, tb, tr, NEXTOP(PredStaticClause->CodeOfPred,ld), cp_ptr);
  WRITE_UNLOCK(pe->PRWLock);
  if (cl == NULL)
    return FALSE;
  rtn = MkDBRefTerm((DBRef)cl);
  if (cl->ClFlags & FactMask) {
    if (!Yap_unify(tb, MkAtomTerm(AtomTrue)) ||
	!Yap_unify(tr, rtn))
      return FALSE;

    if (pe->ArityOfPE) {
      Functor f = FunctorOfTerm(th);
      UInt arity = ArityOfFunctor(f), i;
      CELL *pt = RepAppl(th)+1;

      for (i=0; i<arity; i++) {
	XREGS[i+1] = pt[i];
      }
      /* don't need no ENV */
      if (first_time) {
	CP = P;
	ENV = YENV;
	YENV = ASP;
	YENV[E_CB] = (CELL) B;
      }
      P = cl->ClCode;
    }
    return TRUE;
  } else {
    Term t;

    while ((t = Yap_FetchTermFromDB(cl->usc.ClSource)) == 0L) {
      if (first_time) {
	if (!Yap_gc(4, YENV, P)) {
	  Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
	  return FALSE;
	}
      } else {
	if (!Yap_gc(5, ENV, CP)) {
	  Yap_Error(OUT_OF_STACK_ERROR, TermNil, Yap_ErrorMessage);
	  return FALSE;
	}
      }
    }
    return(Yap_unify(th, ArgOfTerm(1,t)) &&
	   Yap_unify(tb, ArgOfTerm(2,t)) &&
	   Yap_unify(tr, rtn));
  }
}

static Int			/* $hidden_predicate(P) */
p_static_clause(void)
{
  PredEntry      *pe;
  Term t1 = Deref(ARG1);

  pe = get_pred(t1, Deref(ARG2), "clause/3");
  if (pe == NULL || EndOfPAEntr(pe))
    return FALSE;
  WRITE_LOCK(pe->PRWLock);
  if(pe->OpcodeOfPred == INDEX_OPCODE) {
    IPred(pe);
  }
  return fetch_next_static_clause(pe, pe->cs.p_code.TrueCodeOfPred, t1, ARG3, ARG4, P, TRUE);
}

static Int			/* $hidden_predicate(P) */
p_nth_clause(void)
{
  PredEntry      *pe;
  Term t1 = Deref(ARG1);
  Term tn = Deref(ARG3);
  LogUpdClause *cl;
  Int ncls;

  if (!IsIntegerTerm(tn))
    return FALSE;
  ncls = IntegerOfTerm(tn);
  pe = get_pred(t1, Deref(ARG2), "clause/3");
  if (pe == NULL || EndOfPAEntr(pe))
    return FALSE;
  WRITE_LOCK(pe->PRWLock);
#if defined(YAPOR) || defined(THREADS)
  WPP = pe;
#endif
  if (!(pe->PredFlags & (SourcePredFlag|LogUpdatePredFlag))) {
    WRITE_UNLOCK(pe->PRWLock);
    return FALSE;
  }
  /* in case we have to index or to expand code */
  if (pe->ModuleOfPred != IDB_MODULE) {
    UInt i;

    for (i = 1; i <= pe->ArityOfPE; i++) {
      XREGS[i] = MkVarTerm();
    }
  } else {
      XREGS[2] = MkVarTerm();
  }
  if(pe->OpcodeOfPred == INDEX_OPCODE) {
    IPred(pe);
  }
  cl = Yap_NthClause(pe, ncls);
  if (cl == NULL) 
    return FALSE;
  if (cl->ClFlags & LogUpdatePredFlag) {
#if defined(YAPOR) || defined(THREADS)
    LOCK(cl->ClLock);
    TRAIL_CLREF(cl);		/* So that fail will erase it */
    INC_CLREF_COUNT(cl);
    UNLOCK(cl->ClLock);
#else
    if (!(cl->ClFlags & InUseMask)) {
      cl->ClFlags |= InUseMask;
      TRAIL_CLREF(cl);	/* So that fail will erase it */
    }
#endif
  }
  return Yap_unify(MkDBRefTerm((DBRef)cl), ARG4);
}

static Int			/* $hidden_predicate(P) */
p_continue_static_clause(void)
{
  PredEntry *pe = (PredEntry *)IntegerOfTerm(Deref(ARG1));
  yamop *ipc = (yamop *)IntegerOfTerm(ARG2);

  WRITE_LOCK(pe->PRWLock);
  return fetch_next_static_clause(pe, ipc, Deref(ARG3), ARG4, ARG5, B->cp_ap, FALSE);
}

#ifdef LOW_PROF

static void
add_code_in_pred(PredEntry *pp) {
  yamop *clcode;

  READ_LOCK(pp->PRWLock);
  /* check if the codeptr comes from the indexing code */
  
  if (pp->PredFlags & (CPredFlag|AsmPredFlag)) {
    char *code_end;
    StaticClause *cl;

    clcode = pp->CodeOfPred;
    cl = ClauseCodeToStaticClause(clcode);
    code_end = (char *)cl + Yap_SizeOfBlock((CODEADDR)cl);
    Yap_inform_profiler_of_clause(clcode, (yamop *)code_end, pp);
    return;
  }
  clcode = pp->cs.p_code.TrueCodeOfPred;
  if (pp->PredFlags & IndexedPredFlag) {
    char *code_end;
    if (pp->PredFlags & LogUpdatePredFlag) {
      LogUpdIndex *cl = ClauseCodeToLogUpdIndex(clcode);
      code_end = (char *)cl + Yap_SizeOfBlock((CODEADDR)cl);
    } else {
      StaticIndex *cl = ClauseCodeToStaticIndex(clcode);
      code_end = (char *)cl + Yap_SizeOfBlock((CODEADDR)cl);
    }
    Yap_inform_profiler_of_clause(clcode, (yamop *)code_end, pp);
  }	      
  clcode = pp->cs.p_code.FirstClause;
  if (clcode != NULL) {
    if (pp->PredFlags & LogUpdatePredFlag) {
      LogUpdClause *cl = ClauseCodeToLogUpdClause(clcode);
      do {
	char *code_end;

	code_end = (char *)cl + Yap_SizeOfBlock((CODEADDR)cl);
	Yap_inform_profiler_of_clause(cl->ClCode, (yamop *)code_end, pp);
	cl = cl->ClNext;
      } while (cl != NULL);
    } else if (pp->PredFlags & DynamicPredFlag) {
      do {
	CODEADDR cl;
	char *code_end;

	cl = (CODEADDR)ClauseCodeToDynamicClause(clcode);
	code_end = cl + Yap_SizeOfBlock((CODEADDR)cl);
	Yap_inform_profiler_of_clause(clcode, (yamop *)code_end, pp);
	if (clcode == pp->cs.p_code.LastClause)
	  break;
	clcode = NextDynamicClause(clcode);
      } while (TRUE);
    } else {
      StaticClause *cl = ClauseCodeToStaticClause(clcode);
      do {
	char *code_end;

	code_end = (char *)cl + Yap_SizeOfBlock((CODEADDR)cl);
	Yap_inform_profiler_of_clause(cl->ClCode, (yamop *)code_end, pp);
	if (cl->ClCode == pp->cs.p_code.FirstClause)
	  break;
	cl = cl->ClNext;
      } while (TRUE);
    }
  }
  READ_UNLOCK(pp->PRWLock); 
}


void
Yap_dump_code_area_for_profiler(void) {
  Int i_table;

  for (i_table = NoOfModules-1; i_table >= 0; --i_table) {
    PredEntry *pp = ModulePred[i_table];
    while (pp != NULL) {
      /*      if (pp->ArityOfPE) {
	fprintf(stderr,"%s/%d %p\n",
		RepAtom(NameOfFunctor(pp->FunctorOfPred))->StrOfAE,
		pp->ArityOfPE,
		pp);
      } else {
	fprintf(stderr,"%s %p\n",
		RepAtom((Atom)(pp->FunctorOfPred))->StrOfAE,
		pp);
		}*/
      add_code_in_pred(pp);
      pp = pp->NextPredOfModule;
    }
  }
  Yap_inform_profiler_of_clause(COMMA_CODE, FAILCODE, RepPredProp(Yap_GetPredPropByFunc(FunctorComma,0)));
  Yap_inform_profiler_of_clause(FAILCODE, FAILCODE+1, RepPredProp(Yap_GetPredPropByAtom(AtomFail,0)));
}

#endif /* LOW_PROF */

static UInt
index_ssz(StaticIndex *x)
{
  UInt sz = Yap_SizeOfBlock((CODEADDR)x);
  x = x->ChildIndex;
  while (x != NULL) {
    sz += index_ssz(x);
    x = x->SiblingIndex;
  }
  return sz;
}

static Int
static_statistics(PredEntry *pe)
{
  UInt sz = 0, cls = 0, isz = 0;
  StaticClause *cl = ClauseCodeToStaticClause(pe->cs.p_code.FirstClause);

  if (pe->cs.p_code.NOfClauses) {
    do {
      cls++;
      sz += Yap_SizeOfBlock((CODEADDR)cl);
      if (cl->ClCode == pe->cs.p_code.LastClause)
	break;
      cl = cl->ClNext;
    } while (TRUE);
  }
  if (pe->cs.p_code.NOfClauses > 1 &&
      pe->cs.p_code.TrueCodeOfPred != pe->cs.p_code.FirstClause) {
    isz = index_ssz(ClauseCodeToStaticIndex(pe->cs.p_code.TrueCodeOfPred));
  }
  return Yap_unify(ARG3, MkIntegerTerm(cls)) &&
    Yap_unify(ARG4, MkIntegerTerm(sz)) &&
    Yap_unify(ARG5, MkIntegerTerm(isz));
}

static Int
p_static_pred_statistics(void)
{
  Term t = Deref(ARG1);
  Term mod = Deref(ARG2);
  PredEntry      *pe;

  if (IsVarTerm(t)) {
    return (FALSE);
  } else if (IsAtomTerm(t)) {
    Atom at = AtomOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByAtom(at, mod));
  } else if (IsApplTerm(t)) {
    Functor         fun = FunctorOfTerm(t);
    pe = RepPredProp(Yap_GetPredPropByFunc(fun, mod));
  } else
    return (FALSE);
  if (pe == NIL)
    return (FALSE);
  if (pe->PredFlags & (DynamicPredFlag|LogUpdatePredFlag|UserCPredFlag|AsmPredFlag|CPredFlag|BinaryTestPredFlag)) {
    /* should use '$recordedp' in this case */
    return FALSE;
  }
  return static_statistics(pe);
}


void 
Yap_InitCdMgr(void)
{
  Yap_InitCPred("$compile_mode", 2, p_compile_mode, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$start_consult", 3, p_startconsult, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$show_consult_level", 1, p_showconslultlev, SafePredFlag);
  Yap_InitCPred("$end_consult", 0, p_endconsult, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$set_spy", 2, p_setspy, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$rm_spy", 2, p_rmspy, SafePredFlag|SyncPredFlag);
  /* gc() may happen during compilation, hence these predicates are
	now unsafe */
  Yap_InitCPred("$compile", 4, p_compile, SyncPredFlag);
  Yap_InitCPred("$compile_dynamic", 5, p_compile_dynamic, SyncPredFlag);
  Yap_InitCPred("$purge_clauses", 2, p_purge_clauses, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$in_use", 2, p_in_use, TestPredFlag | SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$is_dynamic", 2, p_is_dynamic, TestPredFlag | SafePredFlag);
  Yap_InitCPred("$is_log_updatable", 2, p_is_log_updatable, TestPredFlag | SafePredFlag);
  Yap_InitCPred("$is_source", 2, p_is_source, TestPredFlag | SafePredFlag);
  Yap_InitCPred("$pred_exists", 2, p_pred_exists, TestPredFlag | SafePredFlag);
  Yap_InitCPred("$number_of_clauses", 3, p_number_of_clauses, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$undefined", 2, p_undefined, SafePredFlag|TestPredFlag);
  Yap_InitCPred("$optimizer_on", 0, p_optimizer_on, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$clean_up_dead_clauses", 0, p_clean_up_dead_clauses, SyncPredFlag);
  Yap_InitCPred("$optimizer_off", 0, p_optimizer_off, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$kill_dynamic", 2, p_kill_dynamic, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$in_this_file_before", 3, p_in_this_f_before, SafePredFlag);
  Yap_InitCPred("$first_clause_in_file", 3, p_first_cl_in_f, SafePredFlag);
  Yap_InitCPred("$mk_cl_not_first", 2, p_mk_cl_not_first, SafePredFlag);
  Yap_InitCPred("$new_multifile", 3, p_new_multifile, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$is_multifile", 2, p_is_multifile, TestPredFlag | SafePredFlag);
  Yap_InitCPred("$is_profiled", 1, p_is_profiled, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$profile_info", 3, p_profile_info, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$profile_reset", 2, p_profile_reset, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$is_call_counted", 1, p_is_call_counted, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$call_count_info", 3, p_call_count_info, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$call_count_set", 6, p_call_count_set, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$call_count_reset", 0, p_call_count_reset, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$toggle_static_predicates_in_use", 0, p_toggle_static_predicates_in_use, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$set_pred_module", 2, p_set_pred_module, SafePredFlag);
  Yap_InitCPred("$parent_pred", 3, p_parent_pred, SafePredFlag);
  Yap_InitCPred("$system_predicate", 2, p_system_pred, SafePredFlag);
  Yap_InitCPred("$hide_predicate", 2, p_hide_predicate, SafePredFlag);
  Yap_InitCPred("$hidden_predicate", 2, p_hidden_predicate, SafePredFlag);
  Yap_InitCPred("$pred_for_code", 5, p_pred_for_code, SyncPredFlag);
  Yap_InitCPred("$current_stack", 1, p_current_stack, SyncPredFlag);
  Yap_InitCPred("$log_update_clause", 4, p_log_update_clause, SyncPredFlag);
  Yap_InitCPred("$continue_log_update_clause", 5, p_continue_log_update_clause, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$log_update_clause", 3, p_log_update_clause0, SyncPredFlag);
  Yap_InitCPred("$continue_log_update_clause", 4, p_continue_log_update_clause0, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$static_clause", 4, p_static_clause, SyncPredFlag);
  Yap_InitCPred("$continue_static_clause", 5, p_continue_static_clause, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("$static_pred_statistics", 5, p_static_pred_statistics, SyncPredFlag);
  Yap_InitCPred("$p_nth_clause", 4, p_nth_clause, SyncPredFlag);
}

