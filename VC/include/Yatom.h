






/*************************************************************************
*									 *
*	 YAP Prolog   %W% %G%
*									 *
*	Yap Prolog was developed at NCCUP - Universidade do Porto	 *
*									 *
* Copyright L.Damas, V.S.Costa and Universidade do Porto 1985-1997	 *
*									 *
**************************************************************************
*									 *
* File:		YAtom.h.m4						 *
* Last rev:	19/2/88							 *
* mods:									 *
* comments:	atom properties header file for YAP			 *
*									 *
*************************************************************************/

/* This code can only be defined *after* including Regs.h!!! */

#if USE_OFFSETS

inline EXTERN Atom AbsAtom (AtomEntry * p);

inline EXTERN Atom
AbsAtom (AtomEntry * p)
{
  return (Atom) (Addr (p) - AtomBase);
}



inline EXTERN AtomEntry *RepAtom (Atom a);

inline EXTERN AtomEntry *
RepAtom (Atom a)
{
  return (AtomEntry *) (AtomBase + Unsigned (a));
}


#else

inline EXTERN Atom AbsAtom (AtomEntry * p);

inline EXTERN Atom
AbsAtom (AtomEntry * p)
{
  return (Atom) (p);
}



inline EXTERN AtomEntry *RepAtom (Atom a);

inline EXTERN AtomEntry *
RepAtom (Atom a)
{
  return (AtomEntry *) (a);
}


#endif

#if USE_OFFSETS_IN_PROPS

inline EXTERN Prop AbsProp (PropEntry * p);

inline EXTERN Prop
AbsProp (PropEntry * p)
{
  return (Prop) (Addr (p) - AtomBase);
}



inline EXTERN PropEntry *RepProp (Prop p);

inline EXTERN PropEntry *
RepProp (Prop p)
{
  return (PropEntry *) (AtomBase + Unsigned (p));
}


#else

inline EXTERN Prop AbsProp (PropEntry * p);

inline EXTERN Prop
AbsProp (PropEntry * p)
{
  return (Prop) (p);
}



inline EXTERN PropEntry *RepProp (Prop p);

inline EXTERN PropEntry *
RepProp (Prop p)
{
  return (PropEntry *) (p);
}


#endif

#if USE_OFFSETS_IN_PROPS

inline EXTERN FunctorEntry *RepFunctorProp (Prop p);

inline EXTERN FunctorEntry *
RepFunctorProp (Prop p)
{
  return (FunctorEntry *) (AtomBase + Unsigned (p));
}



inline EXTERN Prop AbsFunctorProp (FunctorEntry * p);

inline EXTERN Prop
AbsFunctorProp (FunctorEntry * p)
{
  return (Prop) (Addr (p) - AtomBase);
}


#else

inline EXTERN FunctorEntry *RepFunctorProp (Prop p);

inline EXTERN FunctorEntry *
RepFunctorProp (Prop p)
{
  return (FunctorEntry *) (p);
}



inline EXTERN Prop AbsFunctorProp (FunctorEntry * p);

inline EXTERN Prop
AbsFunctorProp (FunctorEntry * p)
{
  return (Prop) (p);
}


#endif


inline EXTERN unsigned int ArityOfFunctor (Functor);

inline EXTERN unsigned int
ArityOfFunctor (Functor Fun)
{
  return (unsigned int) (((FunctorEntry *) Fun)->ArityOfFE);
}



inline EXTERN Atom NameOfFunctor (Functor);

inline EXTERN Atom
NameOfFunctor (Functor Fun)
{
  return (Atom) (((FunctorEntry *) Fun)->NameOfFE);
}




inline EXTERN PropFlags IsFunctorProperty (int);

inline EXTERN PropFlags
IsFunctorProperty (int flags)
{
  return (PropFlags) ((flags == FunctorProperty));
}



/* summary of property codes used

	00 00	predicate entry
	80 00	db property
	bb 00	functor entry 
	ff df	sparse functor
	ff ex	arithmetic property
	ff f7   array
	ff fa   module property
	ff fb   blackboard property
	ff fc	value property
	ff ff	op property
*/

/*	Module property 						*/
typedef struct
{
  Prop NextOfPE;		/* used to chain properties             */
  PropFlags KindOfPE;		/* kind of property                     */
  SMALLUNSGN IndexOfMod;	/* indec in module table                */
}
ModEntry;

#if USE_OFFSETS_IN_PROPS

inline EXTERN ModEntry *RepModProp (Prop p);

inline EXTERN ModEntry *
RepModProp (Prop p)
{
  return (ModEntry *) (AtomBase + Unsigned (p));
}



inline EXTERN Prop AbsModProp (ModEntry * p);

inline EXTERN Prop
AbsModProp (ModEntry * p)
{
  return (Prop) (Addr (p) - AtomBase);
}


#else

inline EXTERN ModEntry *RepModProp (Prop p);

inline EXTERN ModEntry *
RepModProp (Prop p)
{
  return (ModEntry *) (p);
}



inline EXTERN Prop AbsModProp (ModEntry * p);

inline EXTERN Prop
AbsModProp (ModEntry * p)
{
  return (Prop) (p);
}


#endif

#define ModProperty	((PropFlags)0xfffa)


inline EXTERN PropFlags IsModProperty (int);

inline EXTERN PropFlags
IsModProperty (int flags)
{
  return (PropFlags) ((flags == ModProperty));
}



/*	    operator property entry structure				*/
typedef struct
{
  Prop NextOfPE;		/* used to chain properties     */
  PropFlags KindOfPE;		/* kind of property             */
#if defined(YAPOR) || defined(THREADS)
  rwlock_t OpRWLock;		/* a read-write lock to protect the entry */
#endif
  BITS16 Prefix, Infix, Posfix;	/* precedences                  */
}
OpEntry;
#if USE_OFFSETS_IN_PROPS

inline EXTERN OpEntry *RepOpProp (Prop p);

inline EXTERN OpEntry *
RepOpProp (Prop p)
{
  return (OpEntry *) (AtomBase + Unsigned (p));
}



inline EXTERN Prop AbsOpProp (OpEntry * p);

inline EXTERN Prop
AbsOpProp (OpEntry * p)
{
  return (Prop) (Addr (p) - AtomBase);
}


#else

inline EXTERN OpEntry *RepOpProp (Prop p);

inline EXTERN OpEntry *
RepOpProp (Prop p)
{
  return (OpEntry *) (p);
}



inline EXTERN Prop AbsOpProp (OpEntry * p);

inline EXTERN Prop
AbsOpProp (OpEntry * p)
{
  return (Prop) (p);
}


#endif
#define	OpProperty  ((PropFlags)0xffff)


inline EXTERN PropFlags IsOpProperty (int);

inline EXTERN PropFlags
IsOpProperty (int flags)
{
  return (PropFlags) ((flags == OpProperty));
}



/* defines related to operator specifications				*/
#define	MaskPrio  0x0fff
#define	DcrlpFlag 0x1000
#define	DcrrpFlag 0x2000

typedef union arith_ret *eval_ret;

/*	    expression property	entry structure			*/
typedef struct
{
  Prop NextOfPE;		/* used to chain properties             */
  PropFlags KindOfPE;		/* kind of property                     */
  unsigned int ArityOfEE;
  BITS16 ENoOfEE;
  BITS16 FlagsOfEE;
  /* operations that implement the expression */
  union
  {
    blob_type (*constant) (eval_ret);
    blob_type (*unary) (Term, eval_ret);
    blob_type (*binary) (Term, Term, eval_ret);
  }
  FOfEE;
}
ExpEntry;
#if USE_OFFSETS_IN_PROPS

inline EXTERN ExpEntry *RepExpProp (Prop p);

inline EXTERN ExpEntry *
RepExpProp (Prop p)
{
  return (ExpEntry *) (AtomBase + Unsigned (p));
}



inline EXTERN Prop AbsExpProp (ExpEntry * p);

inline EXTERN Prop
AbsExpProp (ExpEntry * p)
{
  return (Prop) (Addr (p) - AtomBase);
}


#else

inline EXTERN ExpEntry *RepExpProp (Prop p);

inline EXTERN ExpEntry *
RepExpProp (Prop p)
{
  return (ExpEntry *) (p);
}



inline EXTERN Prop AbsExpProp (ExpEntry * p);

inline EXTERN Prop
AbsExpProp (ExpEntry * p)
{
  return (Prop) (p);
}


#endif
#define	ExpProperty  0xffe0

/* only unary and binary expressions are acceptable */

inline EXTERN PropFlags IsExpProperty (int);

inline EXTERN PropFlags
IsExpProperty (int flags)
{
  return (PropFlags) ((flags == ExpProperty));
}




/*		value property entry structure				*/
typedef struct
{
  Prop NextOfPE;		/* used to chain properties             */
  PropFlags KindOfPE;		/* kind of property                     */
#if defined(YAPOR) || defined(THREADS)
  rwlock_t VRWLock;		/* a read-write lock to protect the entry */
#endif
  Term ValueOfVE;		/* (atomic) value associated with the atom */
}
ValEntry;
#if USE_OFFSETS_IN_PROPS

inline EXTERN ValEntry *RepValProp (Prop p);

inline EXTERN ValEntry *
RepValProp (Prop p)
{
  return (ValEntry *) (AtomBase + Unsigned (p));
}



inline EXTERN Prop AbsValProp (ValEntry * p);

inline EXTERN Prop
AbsValProp (ValEntry * p)
{
  return (Prop) (Addr (p) - AtomBase);
}


#else

inline EXTERN ValEntry *RepValProp (Prop p);

inline EXTERN ValEntry *
RepValProp (Prop p)
{
  return (ValEntry *) (p);
}



inline EXTERN Prop AbsValProp (ValEntry * p);

inline EXTERN Prop
AbsValProp (ValEntry * p)
{
  return (Prop) (p);
}


#endif
#define	ValProperty ((PropFlags)0xfffc)


inline EXTERN PropFlags IsValProperty (int);

inline EXTERN PropFlags
IsValProperty (int flags)
{
  return (PropFlags) ((flags == ValProperty));
}



/*	    predicate property entry structure				*/
/*  BasicPreds are things like var, nonvar, atom ...which are implemented
	    through dedicated machine instructions. In this case the 8 lower
	    bits of PredFlags are used to hold the machine instruction code
	    for	the pred.
    C_Preds are	things write, read, ...	implemented in C. In this case
	    CodeOfPred holds the address of the	correspondent C-function.
*/
typedef enum
{
  SourcePredFlag = 0x400000L,	/* static predicate with source declaration */
  MetaPredFlag = 0x200000L,	/* predicate subject to a meta declaration */
  SyncPredFlag = 0x100000L,	/* has to synch before it can execute */
  UserCPredFlag = 0x080000L,	/* CPred defined by the user    */
  MultiFileFlag = 0x040000L,	/* is multi-file                */
  FastPredFlag = 0x020000L,	/* is "compiled"                */
  TestPredFlag = 0x010000L,	/* is a test (optim. comit)     */
  BasicPredFlag = 0x008000L,	/* inline                       */
  StandardPredFlag = 0x004000L,	/* system predicate             */
  DynamicPredFlag = 0x002000L,	/* dynamic predicate            */
  CPredFlag = 0x001000L,	/* written in C                 */
  SafePredFlag = 0x000800L,	/* does not alter arguments     */
  CompiledPredFlag = 0x000400L,	/* is static                    */
  IndexedPredFlag = 0x000200L,	/* has indexing code            */
  SpiedPredFlag = 0x000100L,	/* is a spy point               */
  BinaryTestPredFlag = 0x000080L,	/* test predicate.              */
#ifdef TABLING
  TabledPredFlag = 0x000040L,	/* is tabled                    */
#endif				/* TABLING */
#ifdef YAPOR
  SequentialPredFlag = 0x000020L,	/* may not create par. choice points! */
#endif				/* YAPOR */
  ProfiledPredFlag = 0x000010L,	/* pred is being profiled        */
  LogUpdatePredFlag = 0x000008L	/* dynamic predicate with log. upd. sem. */
}
pred_flag;

/* profile data */
typedef struct
{
#if defined(YAPOR) || defined(THREADS)
  lockvar lock;			/* a simple lock to protect this entry */
#endif
  Int NOfEntries;		/* nbr of times head unification succeeded */
  Int NOfHeadSuccesses;		/* nbr of times head unification succeeded */
  Int NOfRetries;		/* nbr of times a clause for the pred
				   was retried */
}
profile_data;

typedef struct
{
  Prop NextOfPE;		/* used to chain properties             */
  PropFlags KindOfPE;		/* kind of property                     */
  unsigned int ArityOfPE;	/* arity of property                    */
  SMALLUNSGN StateOfPred;	/* actual state of predicate            */
  CODEADDR CodeOfPred;		/* code address                         */
  CODEADDR TrueCodeOfPred;	/* if needing to spy or to lock         */
  Functor FunctorOfPred;	/* functor for Predicate                */
  CODEADDR FirstClause, LastClause;
  CELL PredFlags;
  Atom OwnerFile;		/* File where the predicate was defined */
#if defined(YAPOR) || defined(THREADS)
  rwlock_t PRWLock;		/* a simple lock to protect this entry */
#endif
#ifdef TABLING
  tab_ent_ptr TableOfPred;
#endif				/* TABLING */
  OPCODE OpcodeOfPred;		/* undefcode, indexcode, spycode, ....  */
  profile_data StatisticsForPred;	/* enable profiling for predicate  */
  SMALLUNSGN ModuleOfPred;	/* module for this definition           */
}
PredEntry;
#define PEProp   ((PropFlags)(0x0000))

#if USE_OFFSETS_IN_PROPS

inline EXTERN PredEntry *RepPredProp (Prop p);

inline EXTERN PredEntry *
RepPredProp (Prop p)
{
  return (PredEntry *) (AtomBase + Unsigned (p));
}



inline EXTERN Prop AbsPredProp (PredEntry * p);

inline EXTERN Prop
AbsPredProp (PredEntry * p)
{
  return (Prop) (Addr (p) - AtomBase);
}


#else

inline EXTERN PredEntry *RepPredProp (Prop p);

inline EXTERN PredEntry *
RepPredProp (Prop p)
{
  return (PredEntry *) (p);
}



inline EXTERN Prop AbsPredProp (PredEntry * p);

inline EXTERN Prop
AbsPredProp (PredEntry * p)
{
  return (Prop) (p);
}


#endif


inline EXTERN PropFlags IsPredProperty (int);

inline EXTERN PropFlags
IsPredProperty (int flags)
{
  return (PropFlags) ((flags == PEProp));
}



/********* maximum number of C-written predicates and cmp funcs ******************/

#define MAX_C_PREDS    360
#define MAX_CMP_FUNCS   20

typedef struct
{
  PredEntry *p;
  CmpPredicate f;
}
cmp_entry;

extern CPredicate c_predicates[MAX_C_PREDS];
extern cmp_entry cmp_funcs[MAX_CMP_FUNCS];


/* Flags for code or dbase entry */
/* There are several flags for code and data base entries */
typedef enum
{
  GcFoundMask = 0x10000,	/* informs this is a dynamic predicate */
  DynamicMask = 0x8000,		/* informs this is a dynamic predicate */
  InUseMask = 0x4000,		/* informs this block is being used */
  ErasedMask = 0x2000,		/* informs this block has been erased */
  IndexMask = 0x1000,		/* informs this is indexing code */
  DBClMask = 0x0800,		/* informs this is a data base structure */
  LogUpdRuleMask = 0x0400,	/* informs the code is for a log upd rule with env */
  LogUpdMask = 0x0200,		/* informs this is a logic update index. */
  StaticMask = 0x0100,		/* dealing with static predicates */
  SpiedMask = 0x0080		/* this predicate is being spied */
/* other flags belong to DB */
}
dbentry_flags;

/* *********************** DBrefs **************************************/

#define KEEP_ENTRY_AGE 1

typedef struct DB_STRUCT
{
  Functor id;			/* allow pointers to this struct to id  */
  /*   as dbref                           */
  Term EntryTerm;		/* cell bound to itself                 */
  SMALLUNSGN Flags;		/* Term Flags                           */
  SMALLUNSGN NOfRefsTo;		/* Number of references pointing here   */
  struct struct_dbentry *Parent;	/* key of DBase reference               */
  CODEADDR Code;		/* pointer to code if this is a clause  */
  struct DB_STRUCT **DBRefs;	/* pointer to other references     */
  struct DB_STRUCT *Prev;	/* Previous element in chain            */
  struct DB_STRUCT *Next;	/* Next element in chain                */
#if defined(YAPOR) || defined(THREADS)
  lockvar lock;			/* a simple lock to protect this entry */
  Int ref_count;		/* how many branches are using this entry */
#endif
#ifdef KEEP_ENTRY_AGE
  Int age;			/* entry's age, negative if from recorda,
				   positive if it was recordz  */
#endif				/* KEEP_ENTRY_AGE */
  CELL Mask;			/* parts that should be cleared         */
  CELL Key;			/* A mask that can be used to check before
				   you unify */
  CELL NOfCells;		/* Size of Term                         */
  CELL Entry;			/* entry point                          */
  Term Contents[MIN_ARRAY];	/* stored term                      */
}
DBStruct;

#define DBStructFlagsToDBStruct(X) ((DBRef)((X) - (CELL) &(((DBRef) NIL)->Flags)))

#if defined(YAPOR) || defined(THREADS)
#define INIT_DBREF_COUNT(X) (X)->ref_count = 0
#define  INC_DBREF_COUNT(X) (X)->ref_count++
#define  DEC_DBREF_COUNT(X) (X)->ref_count--
#define     DBREF_IN_USE(X) ((X)->ref_count != 0)
#else
#define INIT_DBREF_COUNT(X)
#define  INC_DBREF_COUNT(X)
#define  DEC_DBREF_COUNT(X)
#define     DBREF_IN_USE(X) ((X)->Flags & InUseMask)
#endif

typedef DBStruct *DBRef;

/* extern Functor FunctorDBRef; */

inline EXTERN int IsDBRefTerm (Term);

inline EXTERN int
IsDBRefTerm (Term t)
{
  return (int) (IsApplTerm (t) && FunctorOfTerm (t) == FunctorDBRef);
}



inline EXTERN Term MkDBRefTerm (DBRef);

inline EXTERN Term
MkDBRefTerm (DBRef p)
{
  return (Term) ((AbsAppl (((CELL *) (p)))));
}



inline EXTERN DBRef DBRefOfTerm (Term t);

inline EXTERN DBRef
DBRefOfTerm (Term t)
{
  return (DBRef) (((DBRef) (RepAppl (t))));
}




inline EXTERN int IsRefTerm (Term);

inline EXTERN int
IsRefTerm (Term t)
{
  return (int) (IsApplTerm (t) && FunctorOfTerm (t) == FunctorDBRef);
}



inline EXTERN CODEADDR RefOfTerm (Term t);

inline EXTERN CODEADDR
RefOfTerm (Term t)
{
  return (CODEADDR) (DBRefOfTerm (t));
}



typedef struct struct_dbentry
{
  Prop NextOfPE;		/* used to chain properties             */
  PropFlags KindOfPE;		/* kind of property                     */
  unsigned int ArityOfDB;	/* kind of property                     */
  Functor FunctorOfDB;		/* functor for this property            */
#if defined(YAPOR) || defined(THREADS)
  rwlock_t DBRWLock;		/* a simple lock to protect this entry */
#endif
  DBRef First;			/* first DBase entry                    */
  DBRef Last;			/* last DBase entry                     */
  SMALLUNSGN ModuleOfDB;	/* module for this definition           */
#ifdef KEEP_ENTRY_AGE
  Int age;			/* age counter                          */
#else
  DBRef FirstNEr;		/* first non-erased DBase entry         */
#endif				/* KEEP_ENTRY_AGE */
}
DBEntry;
typedef DBEntry *DBProp;
#define	DBProperty	   ((PropFlags)0x8000)

typedef struct
{
  Prop NextOfPE;		/* used to chain properties             */
  PropFlags KindOfPE;		/* kind of property                     */
  unsigned int ArityOfDB;	/* kind of property                     */
  Functor FunctorOfDB;		/* functor for this property            */
#if defined(YAPOR) || defined(THREADS)
  rwlock_t DBRWLock;		/* a simple lock to protect this entry */
#endif
  DBRef First;			/* first DBase entry                    */
  DBRef Last;			/* last DBase entry                     */
  SMALLUNSGN ModuleOfDB;	/* module for this definition           */
  Int NOfEntries;		/* age counter                          */
  DBRef Index;			/* age counter                          */
}
LogUpdDBEntry;
typedef LogUpdDBEntry *LogUpdDBProp;
#define	LogUpdDBBit        0x1
#define	CodeDBBit          0x2

#define	LogUpdDBProperty   ((PropFlags)(0x8000|LogUpdDBBit))
#define	CodeLogUpdDBProperty   (DBProperty|LogUpdDBBit|CodeDBBit)
#define	CodeDBProperty     (DBProperty|CodeDBBit)


inline EXTERN PropFlags IsDBProperty (int);

inline EXTERN PropFlags
IsDBProperty (int flags)
{
  return (PropFlags) (((flags & ~(LogUpdDBBit | CodeDBBit)) == DBProperty));
}



#if USE_OFFSETS_IN_PROPS

inline EXTERN DBProp RepDBProp (Prop p);

inline EXTERN DBProp
RepDBProp (Prop p)
{
  return (DBProp) (AtomBase + Unsigned (p));
}



inline EXTERN Prop AbsDBProp (DBProp p);

inline EXTERN Prop
AbsDBProp (DBProp p)
{
  return (Prop) (Addr (p) - AtomBase);
}


#else

inline EXTERN DBProp RepDBProp (Prop p);

inline EXTERN DBProp
RepDBProp (Prop p)
{
  return (DBProp) (p);
}



inline EXTERN Prop AbsDBProp (DBProp p);

inline EXTERN Prop
AbsDBProp (DBProp p)
{
  return (Prop) (p);
}


#endif


/* These are the actual flags for DataBase terms */
typedef enum
{
  DBAtomic = 0x1,
  DBVar = 0x2,
  DBNoVars = 0x4,
  DBComplex = 0x8,
  DBCode = 0x10,
  DBNoCode = 0x20,
  DBWithRefs = 0x40
}
db_term_flags;

#define MaxModules	255
extern SMALLUNSGN CurrentModule;

typedef struct
{
  Prop NextOfPE;		/* used to chain properties                */
  PropFlags KindOfPE;		/* kind of property                        */
  Atom KeyOfBB;			/* functor for this property               */
  DBRef Element;		/* blackboard element                      */
#if defined(YAPOR) || defined(THREADS)
  rwlock_t BBRWLock;		/* a read-write lock to protect the entry */
#endif
  SMALLUNSGN ModuleOfBB;	/* module for this definition             */
}
BlackBoardEntry;
typedef BlackBoardEntry *BBProp;

#if USE_OFFSETS_IN_PROPS

inline EXTERN BlackBoardEntry *RepBBProp (Prop p);

inline EXTERN BlackBoardEntry *
RepBBProp (Prop p)
{
  return (BlackBoardEntry *) (AtomBase + Unsigned (p));
}



inline EXTERN Prop AbsBBProp (BlackBoardEntry * p);

inline EXTERN Prop
AbsBBProp (BlackBoardEntry * p)
{
  return (Prop) (Addr (p) - AtomBase);
}


#else

inline EXTERN BlackBoardEntry *RepBBProp (Prop p);

inline EXTERN BlackBoardEntry *
RepBBProp (Prop p)
{
  return (BlackBoardEntry *) (p);
}



inline EXTERN Prop AbsBBProp (BlackBoardEntry * p);

inline EXTERN Prop
AbsBBProp (BlackBoardEntry * p)
{
  return (Prop) (p);
}


#endif

#define BBProperty	((PropFlags)0xfffb)


inline EXTERN PropFlags IsBBProperty (int);

inline EXTERN PropFlags
IsBBProperty (int flags)
{
  return (PropFlags) ((flags == BBProperty));
}




/*		array property entry structure				*/
/*		first case is for dynamic arrays */
typedef struct
{
  Prop NextOfPE;		/* used to chain properties             */
  PropFlags KindOfPE;		/* kind of property                     */
  Int ArrayEArity;		/* Arity of Array (positive)            */
#if defined(YAPOR) || defined(THREADS)
  rwlock_t ArRWLock;		/* a read-write lock to protect the entry */
#endif
  Term ValueOfVE;		/* Pointer to the actual array          */
}
ArrayEntry;

/* second case is for static arrays */

/* first, the valid types */
typedef enum
{
  array_of_ints,
  array_of_chars,
  array_of_uchars,
  array_of_doubles,
  array_of_ptrs,
  array_of_atoms,
  array_of_dbrefs,
  array_of_terms
}
static_array_types;

typedef union
{
  Int *ints;
  char *chars;
  unsigned char *uchars;
  Float *floats;
  AtomEntry **ptrs;
  Term *atoms;
  Term *dbrefs;
  DBRef *terms;
}
statarray_elements;

/* next, the actual data structure */
typedef struct
{
  Prop NextOfPE;		/* used to chain properties             */
  PropFlags KindOfPE;		/* kind of property                     */
  Int ArrayEArity;		/* Arity of Array (negative)            */
#if defined(YAPOR) || defined(THREADS)
  rwlock_t ArRWLock;		/* a read-write lock to protect the entry */
#endif
  static_array_types ArrayType;	/* Type of Array Elements.              */
  statarray_elements ValueOfVE;	/* Pointer to the Array itself  */
}
StaticArrayEntry;


#if USE_OFFSETS_IN_PROPS

inline EXTERN ArrayEntry *RepArrayProp (Prop p);

inline EXTERN ArrayEntry *
RepArrayProp (Prop p)
{
  return (ArrayEntry *) (AtomBase + Unsigned (p));
}



inline EXTERN Prop AbsArrayProp (ArrayEntry * p);

inline EXTERN Prop
AbsArrayProp (ArrayEntry * p)
{
  return (Prop) (Addr (p) - AtomBase);
}



inline EXTERN StaticArrayEntry *RepStaticArrayProp (Prop p);

inline EXTERN StaticArrayEntry *
RepStaticArrayProp (Prop p)
{
  return (StaticArrayEntry *) (AtomBase + Unsigned (p));
}



inline EXTERN Prop AbsStaticArrayProp (StaticArrayEntry * p);

inline EXTERN Prop
AbsStaticArrayProp (StaticArrayEntry * p)
{
  return (Prop) (Addr (p) - AtomBase);
}


#else

inline EXTERN ArrayEntry *RepArrayProp (Prop p);

inline EXTERN ArrayEntry *
RepArrayProp (Prop p)
{
  return (ArrayEntry *) (p);
}



inline EXTERN Prop AbsArrayProp (ArrayEntry * p);

inline EXTERN Prop
AbsArrayProp (ArrayEntry * p)
{
  return (Prop) (p);
}



inline EXTERN StaticArrayEntry *RepStaticArrayProp (Prop p);

inline EXTERN StaticArrayEntry *
RepStaticArrayProp (Prop p)
{
  return (StaticArrayEntry *) (p);
}



inline EXTERN Prop AbsStaticArrayProp (StaticArrayEntry * p);

inline EXTERN Prop
AbsStaticArrayProp (StaticArrayEntry * p)
{
  return (Prop) (p);
}


#endif
#define	ArrayProperty ((PropFlags)0xfff7)


inline EXTERN int ArrayIsDynamic (ArrayEntry *);

inline EXTERN int
ArrayIsDynamic (ArrayEntry * are)
{
  return (int) (((are)->ArrayEArity > 0));
}




inline EXTERN PropFlags IsArrayProperty (int);

inline EXTERN PropFlags
IsArrayProperty (int flags)
{
  return (PropFlags) ((flags == ArrayProperty));
}



/* Proto types */

/* cdmgr.c */
int STD_PROTO (RemoveIndexation, (PredEntry *));

/* dbase.c */
void STD_PROTO (ErDBE, (DBRef));
DBRef STD_PROTO (StoreTermInDB, (Term, int));
Term STD_PROTO (FetchTermFromDB, (DBRef, int));
void STD_PROTO (ReleaseTermFromDB, (DBRef));

/* .c */
CODEADDR STD_PROTO (PredIsIndexable, (PredEntry *));

/* init.c */
Atom STD_PROTO (GetOp, (OpEntry *, int *, int));

#ifdef XX_ADTDEFS_C
#ifndef inline

/* look property list of atom a for kind  */
EXTERN inline Prop
GetAProp (a, kind)
     Atom a;
     PropFlags kind;
{
  register PropEntry *pp = RepProp (RepAtom (a)->PropOfAE);
  while (!EndOfPAEntr (pp) && pp->KindOfPE != kind)
    pp = RepProp (pp->NextOfPE);
  return (AbsProp (pp));
}

/* get predicate entry for ap/arity; create it if neccessary.		*/
EXTERN inline Prop
PredProp (ap, arity)
     Atom ap;
     unsigned int arity;
{
  Prop p0;
  PredEntry *p = RepPredProp (p0 = RepAtom (ap)->PropOfAE);
  while (p0 && (p->KindOfPE != 00 || p->ArityOfPE != arity ||
		(p->ModuleOfPred && p->ModuleOfPred != CurrentModule)))
    p = RepPredProp (p0 = p->NextOfPE);
  if (p0)
    return (p0);
  YAPEnterCriticalSection ();
  p = (PredEntry *) AllocAtomSpace (sizeof (*p));
  p->KindOfPE = PEProp;
  p->ArityOfPE = arity;
  p->FirstClause = p->LastClause = NIL;
  p->PredFlags = 0L;
  p->StateOfPred = 0;
  p->OwnerFile = AtomNil;
  p->ModuleOfPred = CurrentModule;
  p->OpcodeOfPred = opcode (_undef_p);
  p->StatisticsForPred.NOfEntries = 0;
  p->StatisticsForPred.NOfHeadSuccesses = 0;
  p->StatisticsForPred.NOfRetries = 0;
  p->TrueCodeOfPred = p->CodeOfPred = (CODEADDR) (&(p->DefaultCodeOfPred));
  if (arity == 0)
    p->FunctorOfPred = (Functor) ap;
  else
    p->FunctorOfPred = MkFunctor (ap, arity);
  p->NextOfPE = RepAtom (ap)->PropOfAE;
  RepAtom (ap)->PropOfAE = p0 = AbsPredProp (p);
  YAPLeaveCriticalSection ();
  return (p0);
}

EXTERN inline Term
GetValue (a)
     Atom a;
{
  Prop p0 = GetAProp (a, ValProperty);
  if (p0 == 0)
    return (MkAtomTerm (AtomNil));
  return (RepValProp (p0)->ValueOfVE);
}


EXTERN inline void
PutValue (a, v)
     Atom a;
     Term v;
{
  Prop p0 = GetAProp (a, ValProperty);
  if (p0)
    RepValProp (p0)->ValueOfVE = v;
  else
    {
      ValEntry *p;
      YAPEnterCriticalSection ();
      p = (ValEntry *) AllocAtomSpace (sizeof (ValEntry));
      p->KindOfPE = ValProperty;
      p->ValueOfVE = v;
      p->NextOfPE = RepAtom (a)->PropOfAE;
      RepAtom (a)->PropOfAE = AbsValProp (p);
      YAPLeaveCriticalSection ();
    }
}

#endif /* inline */
#else
/* vsc: redefined to GetAProp to avoid conflicts with Windows header files */
Prop STD_PROTO (GetAProp, (Atom, PropFlags));
Prop STD_PROTO (LockedGetAProp, (AtomEntry *, PropFlags));
Prop STD_PROTO (PredProp, (Atom, unsigned int));
#endif /* ADTDEFS_C */


#if defined(YAPOR) || defined(THREADS)
void STD_PROTO (ReleasePreAllocCodeSpace, (ADDR));
#else
#define ReleasePreAllocCodeSpace(x)
#endif
