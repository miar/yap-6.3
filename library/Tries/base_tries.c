/*********************************************
  File:     base_tries.c
  Author:   Ricardo Rocha
  Comments: Tries base module for Yap Prolog
  version:  $ID$
*********************************************/



/* -------------------------- */
/*          Includes          */
/* -------------------------- */

#include <YapInterface.h>
#include <stdio.h>
#include <string.h>
#include "base_tries.h"



/* -------------------------- */
/*      Local Procedures      */
/* -------------------------- */

static TrNode   put_trie(TrNode node, YAP_Term entry);
static TrNode   check_trie(TrNode node, YAP_Term entry);
static YAP_Term get_trie(TrNode node, YAP_Term *stack_list, TrNode *cur_node);
static void     remove_trie(TrNode node);
static void     free_child_nodes(TrNode node);
static TrNode   copy_child_nodes(TrNode parent_dest, TrNode node_source);
static void     traverse_tries_join(TrNode parent_dest, TrNode parent_source);
static void     traverse_tries(TrNode parent_dest, TrNode parent_source);
static void     traverse_trie_usage(TrNode node, YAP_Int depth);
static void     traverse_trie_save(TrNode node, FILE *file, int float_block);
static void     traverse_trie_load(TrNode parent, FILE *file);
static void     traverse_trie_print(TrNode node, YAP_Int *arity, char *str, int str_index, int mode);



/* -------------------------- */
/*       Local Variables      */
/* -------------------------- */

static TrEngine CURRENT_TRIE_ENGINE;
static YAP_Int CURRENT_TRIE_MODE, CURRENT_DEPTH, CURRENT_INDEX, USAGE_ENTRIES, USAGE_NODES, USAGE_VIRTUAL_NODES;
static YAP_Term AUXILIARY_TERM_STACK[AUXILIARY_TERM_STACK_SIZE];
static YAP_Term *stack_args, *stack_args_base, *stack_vars, *stack_vars_base;
static YAP_Functor FunctorComma;
static void (*DATA_SAVE_FUNCTION)(TrNode, FILE *);
static void (*DATA_LOAD_FUNCTION)(TrNode, YAP_Int, FILE *);
static void (*DATA_PRINT_FUNCTION)(TrNode);
static void (*DATA_DESTRUCT_FUNCTION)(TrNode);
static void (*DATA_JOIN_FUNCTION)(TrNode, TrNode);



/* -------------------------- */
/*     Inline Procedures      */
/* -------------------------- */

static inline
TrNode trie_node_check_insert(TrNode parent, YAP_Term t) {
  TrNode child;

  CURRENT_DEPTH++;
  child = TrNode_child(parent);
  if (child == NULL) {
    new_trie_node(child, t, parent, NULL, NULL, NULL);
    TrNode_child(parent) = child;
  } else if (IS_HASH_NODE(child)) {
    TrHash hash;
    TrNode *bucket;
    int count;
    hash = (TrHash) child;
    bucket = TrHash_bucket(hash, HASH_TERM(t, TrHash_seed(hash)));
    child = *bucket;
    count = 0;
    while (child) {
      if (TrNode_entry(child) == t)
        return child;
      count++;
      child = TrNode_next(child);
    } while (child);
    TrHash_num_nodes(hash)++;
    new_trie_node(child, t, parent, NULL, *bucket, AS_TR_NODE_NEXT(bucket));
    if (*bucket)
      TrNode_previous(*bucket) = child;
    *bucket = child;
    if (count > MAX_NODES_PER_BUCKET && TrHash_num_nodes(hash) > TrHash_num_buckets(hash)) {
      /* expand trie hash */
      TrNode chain, next, *first_bucket, *new_bucket;
      int seed;
      first_bucket = TrHash_buckets(hash);
      bucket = first_bucket + TrHash_num_buckets(hash);
      TrHash_num_buckets(hash) *= 2;
      new_hash_buckets(hash, TrHash_num_buckets(hash)); 
      seed = TrHash_num_buckets(hash) - 1;
      do {
        if (*--bucket) {
          chain = *bucket;
          do {
            new_bucket = TrHash_bucket(hash, HASH_TERM(TrNode_entry(chain), seed));
            next = TrNode_next(chain);
            TrNode_next(chain) = *new_bucket;
	    TrNode_previous(chain) = AS_TR_NODE_NEXT(bucket);
	    if (*new_bucket)
	      TrNode_previous(*new_bucket) = chain;
            *new_bucket = chain;
            chain = next;
          } while (chain);
        }
      } while (bucket != first_bucket);
      free_hash_buckets(first_bucket, TrHash_num_buckets(hash) / 2);
    }
  } else {
    int count = 0;
    do {
      if (TrNode_entry(child) == t)
        return child;
      count++;
      child = TrNode_next(child);
    } while (child);
    new_trie_node(child, t, parent, NULL, TrNode_child(parent), NULL);
    TrNode_previous(TrNode_child(parent)) = child;
    if (++count > MAX_NODES_PER_TRIE_LEVEL) {
      /* alloc a new trie hash */
      TrHash hash;
      TrNode chain, next, *bucket;
      new_trie_hash(hash, count, BASE_HASH_BUCKETS);
      chain = child;
      do {
        bucket = TrHash_bucket(hash, HASH_TERM(TrNode_entry(chain), BASE_HASH_BUCKETS - 1));
        next = TrNode_next(chain);
        TrNode_next(chain) = *bucket;
        TrNode_previous(chain) = AS_TR_NODE_NEXT(bucket);
	if (*bucket)
	  TrNode_previous(*bucket) = chain;
        *bucket = chain;
        chain = next;
      } while (chain);
      TrNode_child(parent) = (TrNode) hash;
    } else
      TrNode_child(parent) = child;
  }
  return child;
}


static inline
TrNode trie_node_insert(TrNode parent, YAP_Term t, TrHash hash) {
  TrNode child;

  CURRENT_DEPTH++;
  if (hash) {
    /* is trie hash */
    TrNode *bucket;
    TrHash_num_nodes(hash)++;
    bucket = TrHash_bucket(hash, HASH_TERM(t, TrHash_seed(hash)));
    new_trie_node(child, t, parent, NULL, *bucket, AS_TR_NODE_NEXT(bucket));
    if (*bucket)
      TrNode_previous(*bucket) = child;
    *bucket = child;
  } else {
    new_trie_node(child, t, parent, NULL, TrNode_child(parent), NULL);
    if (TrNode_child(parent))
      TrNode_previous(TrNode_child(parent)) = child;
    TrNode_child(parent) = child;
  }
  return child;
}


static inline
TrNode trie_node_check(TrNode parent, YAP_Term t) {
  TrNode child;

  child = TrNode_child(parent);
  if (IS_HASH_NODE(child)) {
    TrHash hash;
    TrNode *bucket;
    hash = (TrHash) child;
    bucket = TrHash_bucket(hash, HASH_TERM(t, TrHash_seed(hash)));
    child = *bucket;
    if (!child)
      return NULL;
  }
  do {
    if (TrNode_entry(child) == t)
      return child;
    child = TrNode_next(child);
  } while (child);
  return NULL;
}



/* -------------------------- */
/*            API             */     
/* -------------------------- */

inline
TrEngine trie_init_module(void) {
  static int init_once = 1;
  TrEngine engine;

  if (init_once) {
    CURRENT_TRIE_MODE = TRIE_MODE_STANDARD;
    FunctorComma = YAP_MkFunctor(YAP_LookupAtom(","), 2);
    init_once = 0;
  }
  new_trie_engine(engine);
  return engine;
}


inline
TrNode trie_open(TrEngine engine) {
  TrNode node;

  CURRENT_TRIE_ENGINE = engine;
  new_trie_node(node, 0, NULL, NULL, TrEngine_trie(engine), AS_TR_NODE_NEXT(&TrEngine_trie(engine)));
  if (TrEngine_trie(engine))
    TrNode_previous(TrEngine_trie(engine)) = node;
  TrEngine_trie(engine) = node;
  INCREMENT_TRIES(CURRENT_TRIE_ENGINE);
  return node;
}


inline
void trie_close(TrEngine engine, TrNode node, void (*destruct_function)(TrNode)) {
  CURRENT_TRIE_ENGINE = engine;
  DATA_DESTRUCT_FUNCTION = destruct_function;
  if (TrNode_child(node))
    free_child_nodes(TrNode_child(node));
  if (TrNode_next(node)) {
    TrNode_previous(TrNode_next(node)) = TrNode_previous(node);
    TrNode_next(TrNode_previous(node)) = TrNode_next(node);
  } else
    TrNode_next(TrNode_previous(node)) = NULL;
  free_trie_node(node);  
  DECREMENT_TRIES(CURRENT_TRIE_ENGINE);
  return;
}


inline
void trie_close_all(TrEngine engine, void (*destruct_function)(TrNode)) {
  while (TrEngine_trie(engine))
    trie_close(engine, TrEngine_trie(engine), destruct_function);
  return;
}


inline
void trie_set_mode(YAP_Int mode) {
  CURRENT_TRIE_MODE = mode;
  return;
}


inline
YAP_Int trie_get_mode(void) {
  return CURRENT_TRIE_MODE;
}


inline
TrNode trie_put_entry(TrEngine engine, TrNode node, YAP_Term entry, YAP_Int *depth) {
  CURRENT_TRIE_ENGINE = engine;
  CURRENT_DEPTH = 0;
  stack_args_base = stack_args = AUXILIARY_TERM_STACK;
  stack_vars_base = stack_vars = AUXILIARY_TERM_STACK + AUXILIARY_TERM_STACK_SIZE - 1;
  node = put_trie(node, entry);
  if (!IS_LEAF_TRIE_NODE(node)) {
    MARK_AS_LEAF_TRIE_NODE(node);
    INCREMENT_ENTRIES(CURRENT_TRIE_ENGINE);
  }
  /* reset var terms */
  while (STACK_NOT_EMPTY(stack_vars++, stack_vars_base)) {
    POP_DOWN(stack_vars);
    *((YAP_Term *)*stack_vars) = *stack_vars;
  }
  if (depth)
    *depth = CURRENT_DEPTH;
  return node;
}


inline
TrNode trie_check_entry(TrNode node, YAP_Term entry) {
  if (!TrNode_child(node))
    return NULL;
  stack_args_base = stack_args = AUXILIARY_TERM_STACK;
  stack_vars_base = stack_vars = AUXILIARY_TERM_STACK + AUXILIARY_TERM_STACK_SIZE - 1;
  node = check_trie(node, entry);
  /* reset var terms */
  while (STACK_NOT_EMPTY(stack_vars++, stack_vars_base)) {
    POP_DOWN(stack_vars);
    *((YAP_Term *)*stack_vars) = *stack_vars;
  }
  return node;
}


inline
YAP_Term trie_get_entry(TrNode node) {
  CURRENT_INDEX = -1;
  stack_vars_base = stack_vars = AUXILIARY_TERM_STACK;
  stack_args_base = stack_args = AUXILIARY_TERM_STACK + AUXILIARY_TERM_STACK_SIZE - 1;
  return get_trie(node, stack_args, &node);
}


inline
void trie_remove_entry(TrEngine engine, TrNode node, void (*destruct_function)(TrNode)) {
  CURRENT_TRIE_ENGINE = engine;
  DATA_DESTRUCT_FUNCTION = destruct_function;
  if (DATA_DESTRUCT_FUNCTION)
    (*DATA_DESTRUCT_FUNCTION)(node);
  DECREMENT_ENTRIES(CURRENT_TRIE_ENGINE);
  remove_trie(node);
  return;
}


inline
void trie_remove_subtree(TrEngine engine, TrNode node, void (*destruct_function)(TrNode)) {
  TrNode parent;

  CURRENT_TRIE_ENGINE = engine;
  DATA_DESTRUCT_FUNCTION = destruct_function;
  parent = TrNode_parent(node);
  free_child_nodes(TrNode_child(parent));
  remove_trie(parent);
  return;
}


inline
void trie_join(TrEngine engine, TrNode node_dest, TrNode node_source, void (*add_function)(TrNode, TrNode)) {
  CURRENT_TRIE_ENGINE = engine;
  DATA_JOIN_FUNCTION = add_function;
  if (TrNode_child(node_dest)) {
    if (TrNode_child(node_source))
      traverse_tries_join(node_dest, node_source);
  } else if (TrNode_child(node_source))
    TrNode_child(node_dest) = copy_child_nodes(node_dest, TrNode_child(node_source));
  return;
}


inline
void trie_add(TrNode node_dest, TrNode node_source, void (*add_function)(TrNode, TrNode)) {
  DATA_JOIN_FUNCTION = add_function;
  if (TrNode_child(node_dest) && TrNode_child(node_source))
    traverse_tries(node_dest, node_source);
  return;
}


inline
void trie_save(TrNode node, FILE *file, void (*save_function)(TrNode, FILE *)) {
  CURRENT_INDEX = -1;
  DATA_SAVE_FUNCTION = save_function;
  if (TrNode_child(node)) {
    fprintf(file, "BEGIN_TRIE ");
    traverse_trie_save(TrNode_child(node), file, 0);
    fprintf(file, "END_TRIE");
  }
  return;
}


inline
TrNode trie_load(TrEngine engine, FILE *file, void (*load_function)(TrNode, YAP_Int, FILE *)) {
  TrNode node;

  CURRENT_TRIE_ENGINE = engine;
  CURRENT_INDEX = -1;
  CURRENT_DEPTH = 0;
  DATA_LOAD_FUNCTION = load_function;
  node = trie_open(engine);
  if (!fscanf(file, "BEGIN_TRIE"))
    traverse_trie_load(node, file);
  return node;
}


inline
void trie_stats(TrEngine engine, YAP_Int *memory, YAP_Int *tries, YAP_Int *entries, YAP_Int *nodes) {
  *memory = TrEngine_memory(engine);
  *tries = TrEngine_tries(engine);
  *entries = TrEngine_entries(engine);
  *nodes = TrEngine_nodes(engine);
  return;
}


inline
void trie_max_stats(TrEngine engine, YAP_Int *memory, YAP_Int *tries, YAP_Int *entries, YAP_Int *nodes) {
  *memory = TrEngine_memory_max(engine);
  *tries = TrEngine_tries_max(engine);
  *entries = TrEngine_entries_max(engine);
  *nodes = TrEngine_nodes_max(engine);
  return;
}


inline
void trie_usage(TrNode node, YAP_Int *entries, YAP_Int *nodes, YAP_Int *virtual_nodes) {
  USAGE_ENTRIES = 0;
  USAGE_NODES = 0;
  USAGE_VIRTUAL_NODES = 0;
  if (TrNode_child(node))
    traverse_trie_usage(TrNode_child(node), 0);
  *entries = USAGE_ENTRIES;
  *nodes = USAGE_NODES;
  *virtual_nodes = USAGE_VIRTUAL_NODES;
  return;
}


inline
void trie_print(TrNode node, void (*print_function)(TrNode)) {
  DATA_PRINT_FUNCTION = print_function;
  if (TrNode_child(node)) {
    YAP_Int arity[100];
    char str[1000];
    arity[0] = 0;
    traverse_trie_print(TrNode_child(node), arity, str, 0, TRIE_PRINT_NORMAL);
  } else
    fprintf(stdout, "(empty)\n");
  return;
}




/* -------------------------- */
/*      Local Procedures      */
/* -------------------------- */

static
TrNode put_trie(TrNode node, YAP_Term entry) {
  YAP_Term t = YAP_Deref(entry);
  if (YAP_IsVarTerm(t)) {
    if (IsTrieVar(t, stack_vars, stack_vars_base)) {
      node = trie_node_check_insert(node, MkTrieVar((stack_vars_base - 1 - (YAP_Term *)t) / 2));
    } else {
      node = trie_node_check_insert(node, MkTrieVar((stack_vars_base - stack_vars) / 2));
      PUSH_UP(stack_vars, t, stack_args);
      *((YAP_Term *)t) = (YAP_Term)stack_vars;
      PUSH_UP(stack_vars, stack_vars, stack_args);
    }
  } else if (YAP_IsAtomTerm(t)) {
    node = trie_node_check_insert(node, t);
  } else if (YAP_IsIntTerm(t)) {
    node = trie_node_check_insert(node, t);
  } else if (YAP_IsFloatTerm(t)) {
    volatile double f;
    volatile YAP_Term *p;
    f = YAP_FloatOfTerm(t);
    p = (YAP_Term *)((void *) &f); /* to avoid gcc warning */
    node = trie_node_check_insert(node, FloatInitTag);
    node = trie_node_check_insert(node, *p);
#ifdef TAG_LOW_BITS_32
    node = trie_node_check_insert(node, *(p + 1));
#endif /* TAG_LOW_BITS_32 */
    node = trie_node_check_insert(node, FloatEndTag);
  } else if (YAP_IsPairTerm(t)) {
    node = trie_node_check_insert(node, PairInitTag);
    if (CURRENT_TRIE_MODE == TRIE_MODE_STANDARD) {
      do {
	node = put_trie(node, YAP_HeadOfTerm(t));
	t = YAP_Deref(YAP_TailOfTerm(t));
      } while (YAP_IsPairTerm(t));
    } else { /* TRIE_MODE_REVERSE */
      YAP_Term *stack_list = stack_args;
      do {
	PUSH_DOWN(stack_args, YAP_HeadOfTerm(t), stack_vars);
	t = YAP_Deref(YAP_TailOfTerm(t));
      } while (YAP_IsPairTerm(t));
      while (STACK_NOT_EMPTY(stack_args, stack_list))
	node = put_trie(node, POP_UP(stack_args));
    }
    node = trie_node_check_insert(node, PairEndTag);
  } else if (YAP_IsApplTerm(t)) {
    YAP_Functor f = YAP_FunctorOfTerm(t);
    if (f == FunctorComma) {
      node = trie_node_check_insert(node, CommaInitTag);
      do {
	node = put_trie(node, YAP_ArgOfTerm(1, t));
	t = YAP_Deref(YAP_ArgOfTerm(2, t));
      } while (YAP_IsApplTerm(t) && YAP_FunctorOfTerm(t) == FunctorComma);
      node = put_trie(node, t);
      node = trie_node_check_insert(node, CommaEndTag);
    } else {
      int i;
      node = trie_node_check_insert(node, ApplTag | ((YAP_Term) f));
      for (i = 1; i <= YAP_ArityOfFunctor(f); i++)
	node = put_trie(node, YAP_ArgOfTerm(i, t));
    }
  } else 
    fprintf(stderr, "\nTries base module: unknown type tag\n");
  
  return node;
}


static
TrNode check_trie(TrNode node, YAP_Term entry) {
  YAP_Term t = YAP_Deref(entry);
  if (YAP_IsVarTerm(t)) {
    if (IsTrieVar(t, stack_vars, stack_vars_base)) {
      if (!(node = trie_node_check(node, MkTrieVar((stack_vars_base - 1 - (YAP_Term *)t) / 2))))
	return NULL;
    } else {
      if (!(node = trie_node_check(node, MkTrieVar((stack_vars_base - stack_vars) / 2))))
	return NULL;
      PUSH_UP(stack_vars, t, stack_args);
      *((YAP_Term *)t) = (YAP_Term)stack_vars;
      PUSH_UP(stack_vars, stack_vars, stack_args);
    }
  } else if (YAP_IsAtomTerm(t)) {
    if (!(node = trie_node_check(node, t)))
      return NULL;
  } else if (YAP_IsIntTerm(t)) {
    if (!(node = trie_node_check(node, t)))
      return NULL;
  } else if (YAP_IsFloatTerm(t)) {
    volatile double f;
    volatile YAP_Term *p;
    f = YAP_FloatOfTerm(t);
    p = (YAP_Term *)((void *) &f); /* to avoid gcc warning */
    if (!(node = trie_node_check(node, FloatInitTag)))
      return NULL;
    if (!(node = trie_node_check(node, *p)))
      return NULL;
#ifdef TAG_LOW_BITS_32
    if (!(node = trie_node_check(node, *(p + 1))))
      return NULL;
#endif /* TAG_LOW_BITS_32 */
    if (!(node = trie_node_check(node, FloatEndTag)))
      return NULL;
  } else if (YAP_IsPairTerm(t)) {
    if (!(node = trie_node_check(node, PairInitTag)))
      return NULL;
    if (CURRENT_TRIE_MODE == TRIE_MODE_STANDARD) {
      do {
	if (!(node = check_trie(node, YAP_HeadOfTerm(t))))
	  return NULL;
	t = YAP_Deref(YAP_TailOfTerm(t));
      } while (YAP_IsPairTerm(t));
    } else { /* TRIE_MODE_REVERSE */
      YAP_Term *stack_list = stack_args;
      do {
	PUSH_DOWN(stack_args, YAP_HeadOfTerm(t), stack_vars);
	t = YAP_Deref(YAP_TailOfTerm(t));
      } while (YAP_IsPairTerm(t));
      while (STACK_NOT_EMPTY(stack_args, stack_list))
	if (!(node = check_trie(node, POP_UP(stack_args))))
	  return NULL;
    }
    if (!(node = trie_node_check(node, PairEndTag)))
      return NULL;
  } else if (YAP_IsApplTerm(t)) {
    YAP_Functor f = YAP_FunctorOfTerm(t);
    if (f == FunctorComma) {
      if (!(node = trie_node_check(node, CommaInitTag)))
	return NULL;
      do {
	if (!(node = check_trie(node, YAP_ArgOfTerm(1, t))))
	  return NULL;
	t = YAP_Deref(YAP_ArgOfTerm(2, t));
      } while (YAP_IsApplTerm(t) && YAP_FunctorOfTerm(t) == FunctorComma);
      if (!(node = check_trie(node, t)))
	return NULL;
      if (!(node = trie_node_check(node, CommaEndTag)))
	return NULL;
    } else {
      int i;
      if (!(node = trie_node_check(node, ApplTag | ((YAP_Term) f))))
	return NULL;
      for (i = 1; i <= YAP_ArityOfFunctor(f); i++)
	if (!(node = check_trie(node, YAP_ArgOfTerm(i, t))))
	  return NULL;
    }
  } else 
    fprintf(stderr, "\nTries base module: unknown type tag\n");
  
  return node;
}


static
YAP_Term get_trie(TrNode node, YAP_Term *stack_mark, TrNode *cur_node) {
  YAP_Term t = (YAP_Term) &t;
  while (TrNode_parent(node)) {
    t = TrNode_entry(node);
    if (YAP_IsVarTerm(t)) {
      int index = TrieVarIndex(t);
      if (index > CURRENT_INDEX) {
        int i;
        stack_vars = &stack_vars_base[index + 1];
        if (stack_vars > stack_args + 1)
          fprintf(stderr, "\nTries base module: term stack full");
        for (i = index; i > CURRENT_INDEX; i--)
          stack_vars_base[i] = 0;
        CURRENT_INDEX = index;
      }
      if (stack_vars_base[index]) {
        t = stack_vars_base[index];
      } else {
        t = YAP_MkVarTerm();
        stack_vars_base[index] = t;
      }
      PUSH_UP(stack_args, t, stack_vars);
    } else if (YAP_IsAtomTerm(t)) {
      PUSH_UP(stack_args, t, stack_vars);
    } else if (YAP_IsIntTerm(t)) {
      PUSH_UP(stack_args, t, stack_vars);
    } else if (YAP_IsPairTerm(t)) {
      if (t == PairEndTag) {
        if (CURRENT_TRIE_MODE == TRIE_MODE_STANDARD) {
          t = YAP_MkAtomTerm(YAP_LookupAtom("[]"));
          PUSH_UP(stack_args, t, stack_vars);
          node = TrNode_parent(node);
          t = get_trie(node, &stack_args[1], &node);
        } else { /* TRIE_MODE_REVERSE */
          node = TrNode_parent(node);
          t = get_trie(node, stack_args, &node);
        }
        PUSH_UP(stack_args, t, stack_vars);
      } else if (t == PairInitTag) {
        YAP_Term t2;
        if (CURRENT_TRIE_MODE == TRIE_MODE_STANDARD) {
          YAP_Term *stack_aux = stack_mark;
          t = *stack_aux--;
          while (STACK_NOT_EMPTY(stack_aux, stack_args)) {
            t2 = *stack_aux--;
            t = YAP_MkPairTerm(t2, t);
          }
          stack_args = stack_mark;
        } else { /* TRIE_MODE_REVERSE */
          t = YAP_MkAtomTerm(YAP_LookupAtom("[]"));
          while (STACK_NOT_EMPTY(stack_args, stack_mark)) {
            t2 = POP_DOWN(stack_args);
            t = YAP_MkPairTerm(t2, t);
          }
        }
        *cur_node = node;
        return t;
      } else if (t == CommaEndTag) {
        node = TrNode_parent(node);
        t = get_trie(node, stack_args, &node);
        PUSH_UP(stack_args, t, stack_vars);
      } else if (t == CommaInitTag) {
        YAP_Term *stack_aux = stack_mark;
        stack_aux--;
        while (STACK_NOT_EMPTY(stack_aux, stack_args)) {
          t = YAP_MkApplTerm(FunctorComma, 2, stack_aux);
          *stack_aux = t;
          stack_aux--;
        }
        stack_args = stack_mark;
        *cur_node = node;
        return t;
      } else if (t == FloatEndTag) {
        volatile double f;
        volatile YAP_Term *p;
        p = (YAP_Term *)((void *) &f); /* to avoid gcc warning */
#ifdef TAG_LOW_BITS_32
        node = TrNode_parent(node);
        *(p + 1) = TrNode_entry(node);
#endif /* TAG_LOW_BITS_32 */
        node = TrNode_parent(node);
        *p = TrNode_entry(node);
        node = TrNode_parent(node); /* ignore FloatInitTag */
        t = YAP_MkFloatTerm(f);
        PUSH_UP(stack_args, t, stack_vars);
      } else if (t == FloatInitTag) {
      }
    } else if (ApplTag & t) {
      YAP_Functor f = (YAP_Functor)(~ApplTag & t);
      int arity = YAP_ArityOfFunctor(f);
      t = YAP_MkApplTerm(f, arity, &stack_args[1]);
      stack_args += arity;
      PUSH_UP(stack_args, t, stack_vars);
    } else
      fprintf(stderr, "\nTries base module: unknown type tag\n");
    node = TrNode_parent(node);
  }
  *cur_node = node;
  return t;
}


static
void remove_trie(TrNode node) {
  TrNode parent = TrNode_parent(node);
  while (parent) {
    if (TrNode_previous(node)) {
      if (IS_HASH_NODE(TrNode_child(parent))) {
	TrHash hash = (TrHash) TrNode_child(parent);
	TrHash_num_nodes(hash)--;
	if (TrHash_num_nodes(hash)) {
	  if (TrNode_next(node)) {
	    TrNode_next(TrNode_previous(node)) = TrNode_next(node);
	    TrNode_previous(TrNode_next(node)) = TrNode_previous(node);
	  } else {
	    TrNode_next(TrNode_previous(node)) = NULL;
	  }
	  free_trie_node(node);
	  return;
	}
	free_hash_buckets(TrHash_buckets(hash), TrHash_num_buckets(hash));
	free_trie_hash(hash);
      } else {
	if (TrNode_next(node)) {
	  TrNode_next(TrNode_previous(node)) = TrNode_next(node);
	  TrNode_previous(TrNode_next(node)) = TrNode_previous(node);
	} else {
	  TrNode_next(TrNode_previous(node)) = NULL;
	}
	free_trie_node(node);
	return;
      }
    } else if (TrNode_next(node)) {
      TrNode_child(parent) = TrNode_next(node);
      TrNode_previous(TrNode_next(node)) = NULL;
      free_trie_node(node);
      return;
    }
    free_trie_node(node);
    node = parent;
    parent = TrNode_parent(node);
  }
  TrNode_child(node) = NULL;
  return;
}


static
void free_child_nodes(TrNode node) {
  if (IS_HASH_NODE(node)) {
    TrNode *first_bucket, *bucket;
    TrHash hash = (TrHash) node;
    first_bucket = TrHash_buckets(hash);
    bucket = first_bucket + TrHash_num_buckets(hash);
    do {
      if (*--bucket)
	free_child_nodes(*bucket);
    } while (bucket != first_bucket);
    free_hash_buckets(first_bucket, TrHash_num_buckets(hash));
    free_trie_hash(hash);
    return;
  }
  if (TrNode_next(node))
    free_child_nodes(TrNode_next(node));
  if (!IS_LEAF_TRIE_NODE(node))
    free_child_nodes(TrNode_child(node));
  else {
    if (DATA_DESTRUCT_FUNCTION)
      (*DATA_DESTRUCT_FUNCTION)(node);
    DECREMENT_ENTRIES(CURRENT_TRIE_ENGINE);
  }
  free_trie_node(node);
  return;
}


static
TrNode copy_child_nodes(TrNode parent_dest, TrNode child_source) {
  TrNode child_dest, next_dest;

  if (IS_HASH_NODE(child_source)) {
    TrNode *bucket_dest, *first_bucket_source, *bucket_source;
    TrHash hash_dest, hash_source;
    hash_source = (TrHash) child_source;
    first_bucket_source = TrHash_buckets(hash_source);
    bucket_source = first_bucket_source + TrHash_num_buckets(hash_source);
    new_trie_hash(hash_dest, TrHash_num_nodes(hash_source), TrHash_num_buckets(hash_source));
    bucket_dest = TrHash_buckets(hash_dest) + TrHash_num_buckets(hash_dest);
    do {
      bucket_dest--;
      if (*--bucket_source) {
	*bucket_dest = copy_child_nodes(parent_dest, *bucket_source);
	TrNode_previous(*bucket_dest) = AS_TR_NODE_NEXT(bucket_dest);
      } else
	*bucket_dest = NULL;
    } while (bucket_source != first_bucket_source);
    return (TrNode) hash_dest;
  }

  if (TrNode_next(child_source))
    next_dest = copy_child_nodes(parent_dest, TrNode_next(child_source));
  else
    next_dest = NULL;
  new_trie_node(child_dest, TrNode_entry(child_source), parent_dest, NULL, next_dest, NULL);
  if (next_dest)
    TrNode_previous(next_dest) = child_dest;
  if (IS_LEAF_TRIE_NODE(child_source)) {
    MARK_AS_LEAF_TRIE_NODE(child_dest);
    INCREMENT_ENTRIES(CURRENT_TRIE_ENGINE);
    (*DATA_JOIN_FUNCTION)(child_dest, child_source);
  } else
    TrNode_child(child_dest) = copy_child_nodes(child_dest, TrNode_child(child_source));
  return child_dest;
}


static
void traverse_tries_join(TrNode parent_dest, TrNode parent_source){
  TrNode child_dest, child_source;

  child_source = TrNode_child(parent_source);
  if (IS_HASH_NODE(child_source)) {
    TrNode *first_bucket_source, *bucket_source;
    TrHash hash_source;
    hash_source = (TrHash) child_source;
    first_bucket_source = TrHash_buckets(hash_source);
    bucket_source = first_bucket_source + TrHash_num_buckets(hash_source);
    do {
      child_source = *--bucket_source;
      while (child_source) {
	/* parent_dest cannot be a leaf node */
	child_dest = trie_node_check(parent_dest, TrNode_entry(child_source));
	if (child_dest) {
	  if (IS_LEAF_TRIE_NODE(child_dest))
	    (*DATA_JOIN_FUNCTION)(child_dest, child_source);
	  else
	    /* child_dest is not a leaf node */
	    traverse_tries_join(child_dest, child_source);
	} else {
	  child_dest = trie_node_check_insert(parent_dest, TrNode_entry(child_source));
	  if (IS_LEAF_TRIE_NODE(child_source)) {
	    MARK_AS_LEAF_TRIE_NODE(child_dest);
	    INCREMENT_ENTRIES(CURRENT_TRIE_ENGINE);
	    (*DATA_JOIN_FUNCTION)(child_dest, child_source);
	  } else
            TrNode_child(child_dest) = copy_child_nodes(child_dest, TrNode_child(child_source));
	}
	child_source = TrNode_next(child_source);
      }
    } while (bucket_source != first_bucket_source);
    return;
  }
  while (child_source) {
    /* parent_dest cannot be a leaf node */
    child_dest = trie_node_check(parent_dest, TrNode_entry(child_source));
    if (child_dest) {
      if (IS_LEAF_TRIE_NODE(child_dest))
	(*DATA_JOIN_FUNCTION)(child_dest, child_source);
      else
	/* child_dest is not a leaf node */
	traverse_tries_join(child_dest, child_source);
    } else {
      child_dest = trie_node_check_insert(parent_dest, TrNode_entry(child_source));
      if (IS_LEAF_TRIE_NODE(child_source)) {
	MARK_AS_LEAF_TRIE_NODE(child_dest);
	INCREMENT_ENTRIES(CURRENT_TRIE_ENGINE);
	(*DATA_JOIN_FUNCTION)(child_dest, child_source);
      } else
        TrNode_child(child_dest) = copy_child_nodes(child_dest, TrNode_child(child_source));
    }
    child_source = TrNode_next(child_source);
  }
  return;
}


static
void traverse_tries(TrNode parent_dest, TrNode parent_source) {
  TrNode child_dest, child_source;

  child_source = TrNode_child(parent_source);
  if (IS_HASH_NODE(child_source)) {
    TrNode *first_bucket_source, *bucket_source;
    TrHash hash_source;
    hash_source = (TrHash) child_source;
    first_bucket_source = TrHash_buckets(hash_source);
    bucket_source = first_bucket_source + TrHash_num_buckets(hash_source);
    do {
      child_source = *--bucket_source;
      while (child_source) {
	/* parent_dest cannot be a leaf node */
	child_dest = trie_node_check(parent_dest, TrNode_entry(child_source));
	if (child_dest) {
	  if (IS_LEAF_TRIE_NODE(child_dest)) {
	    if (IS_LEAF_TRIE_NODE(child_source))
	      (*DATA_JOIN_FUNCTION)(child_dest, child_source);
	  } else
	    /* child_dest is not a leaf node */
	    traverse_tries(child_dest, child_source);
	}
	child_source = TrNode_next(child_source);
      }
    } while (bucket_source != first_bucket_source);
    return;
  }
  while (child_source) {
    /* parent_dest cannot be a leaf node */
    child_dest = trie_node_check(parent_dest, TrNode_entry(child_source));
    if (child_dest) {
      if (IS_LEAF_TRIE_NODE(child_dest)) {
	if (IS_LEAF_TRIE_NODE(child_source))
	  (*DATA_JOIN_FUNCTION)(child_dest, child_source);
      } else
	/* child_dest is not a leaf node */
	traverse_tries(child_dest, child_source);
    }
    child_source = TrNode_next(child_source);
  }
  return;
}


static
void traverse_trie_usage(TrNode node, YAP_Int depth) {
  if (IS_HASH_NODE(node)) {
    TrNode *first_bucket, *bucket;
    TrHash hash;
    hash = (TrHash) node;
    first_bucket = TrHash_buckets(hash);
    bucket = first_bucket + TrHash_num_buckets(hash);
    do {
      if (*--bucket) {
        node = *bucket;
        traverse_trie_usage(node, depth);
      }
    } while (bucket != first_bucket);
    return;
  }

  USAGE_NODES++;
  if (TrNode_next(node))
    traverse_trie_usage(TrNode_next(node), depth);
  depth++;
  if (!IS_LEAF_TRIE_NODE(node)) {
    traverse_trie_usage(TrNode_child(node), depth);
  } else {
    USAGE_ENTRIES++;
    USAGE_VIRTUAL_NODES+= depth;
  }
  return;
}


static
void traverse_trie_save(TrNode node, FILE *file, int float_block) {
  YAP_Term t;

  if (IS_HASH_NODE(node)) {
    TrNode *first_bucket, *bucket;
    TrHash hash;
    hash = (TrHash) node;
    fprintf(file, "%lu %d ", HASH_SAVE_MARK, TrHash_num_buckets(hash));
    first_bucket = TrHash_buckets(hash);
    bucket = first_bucket + TrHash_num_buckets(hash);
    do {
      if (*--bucket) {
        node = *bucket;
	traverse_trie_save(node, file, float_block);
      }
    } while (bucket != first_bucket);
    return;
  }

  if (TrNode_next(node))
    traverse_trie_save(TrNode_next(node), file, float_block);

  t = TrNode_entry(node);
  if (float_block) {
    float_block--;
    fprintf(file, "%lu %lu ", FLOAT_SAVE_MARK, t);
  } else if (YAP_IsPairTerm(t)) {
    if (t == FloatInitTag) {
#ifdef TAG_LOW_BITS_32
      float_block++;
#endif /* TAG_LOW_BITS_32 */
      float_block ++;
    }
    fprintf(file, "%lu ", t);
  } else if (YAP_IsVarTerm(t) || YAP_IsIntTerm(t))
    fprintf(file, "%lu ", t);
  else {
    int index;
    for (index = 0; index <= CURRENT_INDEX; index++)
      if (AUXILIARY_TERM_STACK[index] == t)
	break;	   
    if (index > CURRENT_INDEX) {
      CURRENT_INDEX++;
      if (CURRENT_INDEX == AUXILIARY_TERM_STACK_SIZE)
	fprintf(stderr, "\nTries base module: term stack full");
      AUXILIARY_TERM_STACK[CURRENT_INDEX] = t;
      if (YAP_IsAtomTerm(t))
	  fprintf(file, "%lu %d %s ", ATOM_SAVE_MARK, index, YAP_AtomName(YAP_AtomOfTerm(t)));
      else  /* (ApplTag & t) */
	fprintf(file, "%lu %d %s %d ", FUNCTOR_SAVE_MARK, index,
		YAP_AtomName(YAP_NameOfFunctor((YAP_Functor)(~ApplTag & t))),
		YAP_ArityOfFunctor((YAP_Functor)(~ApplTag & t)));
    } else
      if (YAP_IsAtomTerm(t))
	fprintf(file, "%lu %d ", ATOM_SAVE_MARK, index);
      else
	fprintf(file, "%lu %d ", FUNCTOR_SAVE_MARK, index);
  }
  if (IS_LEAF_TRIE_NODE(node)) {
    fprintf(file, "- ");
    if (DATA_SAVE_FUNCTION)
      (*DATA_SAVE_FUNCTION)(node, file);
  }
  else {
    traverse_trie_save(TrNode_child(node), file, float_block);
    fprintf(file, "- ");
  }
  return;
}


static
void traverse_trie_load(TrNode parent, FILE *file) {
  TrHash hash = NULL;
  YAP_Term t;

  if (!fscanf(file, "%lu", &t)) {
    MARK_AS_LEAF_TRIE_NODE(parent);
    INCREMENT_ENTRIES(CURRENT_TRIE_ENGINE);
    if (DATA_LOAD_FUNCTION)
      (*DATA_LOAD_FUNCTION)(parent, CURRENT_DEPTH, file);
    CURRENT_DEPTH--;
    return;
  }
  if (t == HASH_SAVE_MARK) {
    /* alloc a new trie hash */
    int num_buckets;
    fscanf(file, "%d", &num_buckets);
    new_trie_hash(hash, 0, num_buckets);
    TrNode_child(parent) = (TrNode) hash;
    fscanf(file, "%lu", &t);
  }
  do {
    TrNode child;
    if (t == ATOM_SAVE_MARK) {
      int index;
      fscanf(file, "%d", &index);
      if (index > CURRENT_INDEX) {
	char atom[1000];
	fscanf(file, "%s", atom);
	AUXILIARY_TERM_STACK[index] = YAP_MkAtomTerm(YAP_LookupAtom(atom));
	CURRENT_INDEX++;
      }
      t = AUXILIARY_TERM_STACK[index];
    } else if (t == FUNCTOR_SAVE_MARK) {
      int index;
      fscanf(file, "%d", &index);
      if (index > CURRENT_INDEX) {
	char atom[1000];
	int arity;
	fscanf(file, "%s %d", atom, &arity);
	AUXILIARY_TERM_STACK[index] = ApplTag | ((YAP_Term) YAP_MkFunctor(YAP_LookupAtom(atom), arity));
	CURRENT_INDEX++;
      }
      t = AUXILIARY_TERM_STACK[index];
    } else if (t == FLOAT_SAVE_MARK)
      fscanf(file, "%lu", &t);
    child = trie_node_insert(parent, t, hash);
    traverse_trie_load(child, file);
  } while (fscanf(file, "%lu", &t));
  CURRENT_DEPTH--;
  return;
}


static
void traverse_trie_print(TrNode node, YAP_Int *arity, char *str, int str_index, int mode) {
  YAP_Term t;
  YAP_Int new_arity[100];

  if (IS_HASH_NODE(node)) {
    TrNode *first_bucket, *bucket;
    TrHash hash;
    hash = (TrHash) node;
    first_bucket = TrHash_buckets(hash);
    bucket = first_bucket + TrHash_num_buckets(hash);
    do {
      if (*--bucket) {
        node = *bucket;
        memcpy(new_arity, arity, sizeof(YAP_Int) * 100);
        traverse_trie_print(node, new_arity, str, str_index, mode);
      }
    } while (bucket != first_bucket);
    return;
  }

  if (TrNode_next(node)) {
    memcpy(new_arity, arity, sizeof(YAP_Int) * 100);
    traverse_trie_print(TrNode_next(node), new_arity, str, str_index, mode);
  }

  t = TrNode_entry(node);
  if (mode == TRIE_PRINT_FLOAT) {
#ifdef TAG_LOW_BITS_32
    arity[arity[0]] = (YAP_Int) t;
    mode = TRIE_PRINT_FLOAT2;
  } else if (mode == TRIE_PRINT_FLOAT2) {
    volatile double f;
    volatile YAP_Term *p;
    p = (YAP_Term *)((void *) &f); /* to avoid gcc warning */
    *(p + 1) = t;
    *p = (YAP_Term) arity[arity[0]];
#else /* TAG_64BITS */
    volatile double f;
    volatile YAP_Term *p;
    p = (YAP_Term *)((void *) &f); /* to avoid gcc warning */
    *p = t;
#endif /* TAG_SCHEME */
    str_index += sprintf(& str[str_index], "%.10f", f);
    mode = TRIE_PRINT_FLOAT_END;
  } else if (mode == TRIE_PRINT_FLOAT_END) {
    arity[0]--;
    while (arity[0]) {
      arity[arity[0]]--;
      if (arity[arity[0]] == 0) {
	str_index += sprintf(& str[str_index], ")");
	arity[0]--;
      } else {
	str_index += sprintf(& str[str_index], ",");
	break;
      }
    }
    mode = TRIE_PRINT_NORMAL;
  } else if (YAP_IsVarTerm(t)) {
    str_index += sprintf(& str[str_index], "VAR%ld", TrieVarIndex(t));
    while (arity[0]) {
      arity[arity[0]]--;
      if (arity[arity[0]] == 0) {
	str_index += sprintf(& str[str_index], ")");
	arity[0]--;
      } else {
	str_index += sprintf(& str[str_index], ",");
	break;
      }
    }
  } else if (YAP_IsAtomTerm(t)) {
    str_index += sprintf(& str[str_index], "%s", YAP_AtomName(YAP_AtomOfTerm(t)));
    while (arity[0]) {
      arity[arity[0]]--;
      if (arity[arity[0]] == 0) {
	str_index += sprintf(& str[str_index], ")");
	arity[0]--;
      } else {
	str_index += sprintf(& str[str_index], ",");
	break;
      }
    }
  } else if (YAP_IsIntTerm(t)) {
    str_index += sprintf(& str[str_index], "%ld", YAP_IntOfTerm(t));
    while (arity[0]) {
      arity[arity[0]]--;
      if (arity[arity[0]] == 0) {
	str_index += sprintf(& str[str_index], ")");
	arity[0]--;
      } else {
	str_index += sprintf(& str[str_index], ",");
	break;
      }
    }
  } else if (YAP_IsPairTerm(t)) {
    if (t == FloatInitTag) {
      mode = TRIE_PRINT_FLOAT;
      arity[0]++;
      arity[arity[0]] = -1;
    } else if (t == PairInitTag) {
      str_index += sprintf(& str[str_index], "[");
      arity[0]++;
      arity[arity[0]] = -1;
    } else if (t == CommaInitTag) {
      str_index += sprintf(& str[str_index], "(");
      arity[0]++;
      arity[arity[0]] = -1;
    } else {
      if (t == PairEndTag)
	str[str_index - 1] = ']';
      else /*   (t == CommaEndTag)   */
	str[str_index - 1] = ')';
      arity[0]--;
      while (arity[0]) {
	arity[arity[0]]--;
	if (arity[arity[0]] == 0) {
	  str_index += sprintf(& str[str_index], ")");
	  arity[0]--;
	} else {
	  str_index += sprintf(& str[str_index], ",");
	  break;
	}
      }
      if (arity[0]) {
	traverse_trie_print(TrNode_child(node), arity, str, str_index, mode);
      } else {
	str[str_index] = 0;
	fprintf(stdout, "%s\n", str);
        if (DATA_PRINT_FUNCTION)
          (*DATA_PRINT_FUNCTION)(node);
      }
      str[str_index - 1] = ',';
      return;
    }
  } else if (ApplTag & t) {
    str_index += sprintf(& str[str_index], "%s(", YAP_AtomName(YAP_NameOfFunctor((YAP_Functor)(~ApplTag & t))));
    arity[0]++;
    arity[arity[0]] = YAP_ArityOfFunctor((YAP_Functor)(~ApplTag & t));
  } else
    fprintf(stderr, "\nTries base module: unknown type tag\n");

  if (arity[0]) {
    traverse_trie_print(TrNode_child(node), arity, str, str_index, mode);
  } else {
    str[str_index] = 0;
    fprintf(stdout, "%s\n", str);
    if (DATA_PRINT_FUNCTION)
      (*DATA_PRINT_FUNCTION)(node);
  }
  return;
}