#include "lockFreeHash.tries.h"

#ifdef INCLUDE_SUBGOAL_LOCK_FREE_HASH_TRIE
#define LFHT_STR                          struct subgoal_trie_node
#define LFHT_STR_PTR                      sg_node_ptr
#define LFHT_USES_ARGS                    , sg_node_ptr parent_node LFHT_USES_REGS
#define LFHT_USES_ARGS1                   sg_node_ptr parent_node LFHT_USES_REGS
#define LFHT_PASS_ARGS                    , parent_node LFHT_PASS_REGS
#define LFHT_ROOT_ADDR                    (&(TrNode_child(parent_node)))
#define LFHT_GET_FIRST_NODE(NODE)         (NODE = (LFHT_STR_PTR) TrNode_child(parent_node))
#define LFHT_NEW_NODE(NODE, ENTRY, NEXT)  {NEW_SUBGOAL_TRIE_NODE(NODE, ENTRY, NULL, parent_node, NEXT);}
#define LFHT_FREE_NODE(PTR)               FREE_SUBGOAL_TRIE_NODE(PTR);
#undef LFHT_STR
#undef LFHT_STR_PTR
#undef LFHT_NEW_NODE
#undef LFHT_FREE_NODE
#undef LFHT_USES_ARGS1
#undef LFHT_PASS_ARGS
#undef LFHT_ROOT_ADDR
#undef LFHT_GET_FIRST_NODE
#endif /* INCLUDE_SUBGOAL_LOCK_FREE_HASH_TRIE */

#ifdef INCLUDE_ANSWER_LOCK_FREE_HASH_TRIE
#define LFHT_STR                          struct answer_trie_node
#define LFHT_STR_PTR                      ans_node_ptr
#define LFHT_USES_ARGS                    , ans_node_ptr parent_node, int instr LFHT_USES_REGS
#define LFHT_USES_ARGS1                   ans_node_ptr parent_node, int instr LFHT_USES_REGS
#define LFHT_PASS_ARGS                    , parent_node, instr LFHT_PASS_REGS
#define LFHT_ROOT_ADDR                    (&(TrNode_child(parent_node)))
#define LFHT_GET_FIRST_NODE(NODE)         (NODE = (LFHT_STR_PTR) TrNode_child(parent_node))
#define LFHT_NEW_NODE(NODE, ENTRY, NEXT)  {NEW_ANSWER_TRIE_NODE(NODE, instr, ENTRY, NULL, parent_node, NEXT);}
#define LFHT_FREE_NODE(PTR)               FREE_ANSWER_TRIE_NODE(PTR)
#undef LFHT_STR
#undef LFHT_STR_PTR
#undef LFHT_NEW_NODE
#undef LFHT_FREE_NODE
#undef LFHT_USES_ARGS1
#undef LFHT_PASS_ARGS
#undef LFHT_ROOT_ADDR
#undef LFHT_GET_FIRST_NODE
#endif /* INCLUDE_ANSWER_LOCK_FREE_HASH_TRIE */

####################################################################################
query_replace (lockFreeHash.tries.h  + lockFreeHash.tries.i) :
   struct subgoal_trie_node       ->   LFHT_STR
   sg_node_ptr                    ->   LFHT_STR_PTR
   NEW_SUBGOAL_TRIE_NODE          ->   LFHT_NEW_NODE
   FREE_SUBGOAL_TRIE_NODE(PTR)    ->   LFHT_FREE_NODE
   BASE_HASH_BUCKETS              ->   LFHT_BUCKET_SIZE
   SHIFT_SIZE                     ->   LFHT_SHIFTS
   V04_                           ->   LFHT_
   long                           ->   LFHT_CELL
   TrNode_entry                   ->   LFHT_NODE_ENTRY                      (DO NOT REPLACE ON CONFIGURATION STUFF)
   NumberOfLowTagBits             ->   LFHT_NrLowTagBits                    (DO NOT REPLACE ON CONFIGURATION STUFF)
   BOOL_CAS                       ->   LFHT_BOOL_CAS                        (DO NOT REPLACE ON lockFreeHash.tries.i)
   USES_REGS                      ->   LFHT_USES_REGS                       (DO NOT REPLACE ON CONFIGURATION STUFF)
   PASS_REGS                      ->   LFHT_PASS_REGS                       (DO NOT REPLACE ON CONFIGURATION STUFF)
   PASS_REGS                      ->   LFHT_PASS_ARGS PASS_REGS

####################################################################################

####################################################################################
External dependencies:
   check the memory manager
####################################################################################
/* answer_trie_check_insert_entry */
static inline LFHT_STR_PTR lfht_check_insert_entry(LFHT_NODE_ENTRY_STR entry LFHT_USES_ARGS) {
  LFHT_STR_PTR first_node;
  LFHT_GET_FIRST_NODE(first_node);
  if (first_node == NULL) {
    LFHT_STR_PTR new_node;
    LFHT_NEW_NODE(new_node, entry, NULL);
    if (LFHT_BOOL_CAS(LFHT_ROOT_ADDR, NULL, new_node))
      return new_node;
    LFHT_FREE_NODE(new_node);
    LFHT_GET_FIRST_NODE(first_node);
  }  
  if (LFHT_IsHashLevel(first_node))
    return lfht_check_insert_bucket_array((LFHT_STR_PTR *) first_node, entry, 0 LFHT_PASS_ARGS);
  return lfht_check_insert_first_chain(first_node, entry, 0 LFHT_PASS_ARGS);
}

/***********************************************************ok upto here **************************/

/* answer_trie_check_insert_first_chain*/
static inline LFHT_STR_PTR lfht_check_insert_first_chain(LFHT_STR_PTR chain_node, LFHT_NODE_ENTRY_STR entry, int count_nodes LFHT_USES_ARGS) {
  if (LFHT_IsEqualEntry(chain_node, entry))
    return chain_node;  
  int cn = count_nodes + 1;
  LFHT_STR_PTR chain_next;
  chain_next = LFHT_NodeNext(chain_node); // HERE

  if (chain_next && !V04_IS_HASH(chain_next))
    return answer_trie_check_insert_first_chain(chain_next, parent_node, t, instr, cn PASS_REGS);  
  
  // chain_next is a hash pointer or the end of the chain
  if (chain_next == NULL) {
    if (cn == MAX_NODES_PER_BUCKET) {
      ans_node_ptr *new_hash;
      ans_node_ptr *bucket;
      V04_ALLOC_BUCKETS(new_hash, NULL, struct answer_trie_node);
      new_hash = (ans_node_ptr *) V04_TAG(new_hash);
      V04_GET_HASH_BUCKET(bucket, new_hash, TrNode_entry(chain_node), 0, struct answer_trie_node);
      V04_SET_HASH_BUCKET(bucket, chain_node, struct answer_trie_node);
      if (BOOL_CAS(&TrNode_next(chain_node), NULL, new_hash)) {
	answer_trie_adjust_chain_nodes(new_hash, TrNode_child(parent_node), chain_node, (- 1) PASS_REGS);
	TrNode_child(parent_node) = (ans_node_ptr) new_hash;
	return answer_trie_check_insert_bucket_array(new_hash, parent_node, t, instr, 0 PASS_REGS);
      } else 
	V04_FREE_TRIE_HASH_BUCKETS(new_hash, bucket, struct answer_trie_node);  
    } else {
      ans_node_ptr new_node; 
      NEW_ANSWER_TRIE_NODE(new_node, instr, t, NULL, parent_node, NULL);
      if (BOOL_CAS(&TrNode_next(chain_node), NULL, new_node)) 
	return new_node;    
      FREE_ANSWER_TRIE_NODE(new_node);
    }
    chain_next = TrNode_next(chain_node);
    if (!V04_IS_HASH(chain_next))
      return answer_trie_check_insert_first_chain(chain_next, parent_node, t, instr, cn PASS_REGS);  
  }
  // chain_next is pointig to an hash which is newer than mine. I must jump to the correct hash
  ans_node_ptr *jump_hash, *prev_hash;
  jump_hash = (ans_node_ptr *) chain_next;
  V04_GET_PREV_HASH(prev_hash, jump_hash, struct answer_trie_node);
  while (prev_hash != NULL) {
    jump_hash = prev_hash;
    V04_GET_PREV_HASH(prev_hash, jump_hash, struct answer_trie_node);
  }
  return answer_trie_check_insert_bucket_array(jump_hash, parent_node, t, instr, 0 PASS_REGS);  
} 



















