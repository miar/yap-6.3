/************************************************************************
**                                                                     **
**                   The YapTab/YapOr/OPTYap systems                   **
**                                                                     **
** YapTab extends the Yap Prolog engine to support sequential tabling  **
** YapOr extends the Yap Prolog engine to support or-parallelism       **
** OPTYap extends the Yap Prolog engine to support or-parallel tabling **
**                                                                     **
**                                                                     **
**      Yap Prolog was developed at University of Porto, Portugal      **
**                                                                     **
************************************************************************/

/************************************************************************
**                          Memory management                          **
************************************************************************/

extern int Yap_page_size;

#ifdef USE_PAGES_MALLOC
#include <sys/shm.h>
#endif /* USE_PAGES_MALLOC */


#define SHMMAX 0x2000000  /* 32 Mbytes: works fine with linux */
/* #define SHMMAX  0x400000 - 4 Mbytes: shmget limit for Mac (?) */
/* #define SHMMAX  0x800000 - 8 Mbytes: shmget limit for Solaris (?) */

#if SIZEOF_INT_P == 4
#define ALIGN	                   3
#define ALIGNMASK                  0xfffffffc
#elif SIZEOF_INT_P == 8
#define ALIGN	                   7
#define ALIGNMASK                  0xfffffff8
#else
#define ALIGN	                   OOOOPPS!!! Unknown Pointer Sizeof
#define ALIGNMASK                  OOOOPPS!!! Unknown Pointer Sizeof
#endif /* SIZEOF_INT_P */

#define ADJUST_SIZE(SIZE)          ((SIZE + ALIGN) & ALIGNMASK)
#define ADJUST_SIZE_TO_PAGE(SIZE)  ((SIZE) - (SIZE) % Yap_page_size + Yap_page_size)
#define PAGE_HEADER(STR)           (pg_hd_ptr)((unsigned long int)STR - (unsigned long int)STR % Yap_page_size)
#define STRUCT_NEXT(STR)           ((STR)->next)
#define UPDATE_STATS(STAT, VALUE)  STAT += VALUE

#ifdef YAPOR
#define LOCK_PAGE_ENTRY(PG_ENT)    LOCK(PgEnt_lock(PG_ENT))
#define UNLOCK_PAGE_ENTRY(PG_ENT)  UNLOCK(PgEnt_lock(PG_ENT))
#else
#define LOCK_PAGE_ENTRY(PG_ENT)
#define UNLOCK_PAGE_ENTRY(PG_ENT)
#endif

#ifdef USE_SYSTEM_MALLOC
/*******************************************************************************************
**                                      USE_SYSTEM_MALLOC                                 **
*******************************************************************************************/
#define ALLOC_BLOCK(STR, SIZE, STR_TYPE)				                   \
        if ((STR = (STR_TYPE *) malloc(SIZE)) == NULL)                                     \
           Yap_Error(FATAL_ERROR, TermNil, "ALLOC_BLOCK: malloc error")
	 
#define FREE_BLOCK(STR)                                                                    \
  free(STR)
#else
/*******************************************************************************************
**                                    ! USE_SYSTEM_MALLOC                                 **
*******************************************************************************************/
#define ALLOC_BLOCK(STR, SIZE, STR_TYPE)                                                   \
        { char *block_ptr;                                                                 \
          if ((block_ptr = Yap_AllocCodeSpace(SIZE + sizeof(CELL))) != NULL)               \
            *block_ptr = 'y';                                                              \
          else if ((block_ptr = (char *) malloc(SIZE + sizeof(CELL))) != NULL)             \
            *block_ptr = 'm';                                                              \
          else                                                                             \
            Yap_Error(FATAL_ERROR, TermNil, "ALLOC_BLOCK: malloc error");                  \
          block_ptr += sizeof(CELL);                                                       \
          STR = (STR_TYPE *) block_ptr;                                                    \
        }
#define FREE_BLOCK(STR)                                                                    \
        { char *block_ptr = (char *)(STR) - sizeof(CELL);                                  \
          if (block_ptr[0] == 'y')                                                         \
            Yap_FreeCodeSpace(block_ptr);                                                  \
          else                                                                             \
            free(block_ptr);                                                               \
        }
#endif /***********************************************************************************/

#define INIT_BUCKETS(BUCKET_PTR, NUM_BUCKETS)                                              \
        { int i; void **init_bucket_ptr; 		                                   \
	  init_bucket_ptr = (void **) BUCKET_PTR;	                                   \
          for (i = NUM_BUCKETS; i != 0; i--){				                   \
            *init_bucket_ptr++ = NULL;					                   \
	  }								                   \
        }
#define ALLOC_BUCKETS(BUCKET_PTR, NUM_BUCKETS)                                             \
       {  void **alloc_bucket_ptr;		                                           \
          ALLOC_BLOCK(alloc_bucket_ptr, NUM_BUCKETS * sizeof(void *), void *);             \
          INIT_BUCKETS(alloc_bucket_ptr, NUM_BUCKETS);		                           \
          BUCKET_PTR = (void *) alloc_bucket_ptr;			                   \
        }
#define FREE_BUCKETS(BUCKET_PTR)                       FREE_BLOCK(BUCKET_PTR)

#ifndef USE_PAGES_MALLOC
/*******************************************************************************************
**                                    ! USE_PAGES_MALLOC                                  **
*******************************************************************************************/
#define MOVE_PAGES(FROM_PG_ENT, TO_PG_ENT)                                                 \
        UPDATE_STATS(PgEnt_strs_in_use(TO_PG_ENT), PgEnt_strs_in_use(FROM_PG_ENT));        \
        PgEnt_strs_in_use(FROM_PG_ENT) = 0
#define DETACH_PAGES(_PG_ENT)                                                              \
        LOCK(PgEnt_lock(GLOBAL##_PG_ENT));                                                 \
        MOVE_PAGES(LOCAL##_PG_ENT, GLOBAL##_PG_ENT);                                       \
        UNLOCK(PgEnt_lock(GLOBAL##_PG_ENT))
#define ATTACH_PAGES(_PG_ENT) 				                                   \
        MOVE_PAGES(GLOBAL##_PG_ENT, LOCAL##_PG_ENT)
#define GET_FREE_STRUCT(STR, STR_TYPE, PG_ENT, EXTRA_PG_ENT)                               \
        LOCK_PAGE_ENTRY(PG_ENT);                                                           \
        UPDATE_STATS(PgEnt_strs_in_use(PG_ENT), 1);                                        \
        UNLOCK_PAGE_ENTRY(PG_ENT);                                                         \
        ALLOC_BLOCK(STR, sizeof(STR_TYPE), STR_TYPE)
#define GET_NEXT_FREE_STRUCT(LOCAL_STR, STR, STR_TYPE, PG_ENT)                             \
        GET_FREE_STRUCT(STR, STR_TYPE, PG_ENT, ___NOT_USED___)
#define PUT_FREE_STRUCT(STR, STR_TYPE, PG_ENT)                                             \
        LOCK_PAGE_ENTRY(PG_ENT);                                                           \
        UPDATE_STATS(PgEnt_strs_in_use(PG_ENT), -1);                                       \
        UNLOCK_PAGE_ENTRY(PG_ENT);                                                         \
        FREE_BLOCK(STR)
#else
/*******************************************************************************************
**                                      USE_PAGES_MALLOC                                  **
*******************************************************************************************/
#define MOVE_PAGES(FROM_PG_ENT, TO_PG_ENT)                                                 \
        if (PgEnt_first(TO_PG_ENT)) {                                                      \
          PgHd_next(PgEnt_last(TO_PG_ENT)) = PgEnt_first(FROM_PG_ENT);                     \
          PgHd_previous(PgEnt_first(FROM_PG_ENT)) = PgEnt_last(TO_PG_ENT);                 \
        } else                                                                             \
          PgEnt_first(TO_PG_ENT) = PgEnt_first(FROM_PG_ENT);                               \
        PgEnt_last(TO_PG_ENT) = PgEnt_last(FROM_PG_ENT);                                   \
        UPDATE_STATS(PgEnt_pages_in_use(TO_PG_ENT), PgEnt_pages_in_use(FROM_PG_ENT));      \
        UPDATE_STATS(PgEnt_strs_in_use(TO_PG_ENT), PgEnt_strs_in_use(FROM_PG_ENT));        \
        PgEnt_first(FROM_PG_ENT) = PgEnt_last(FROM_PG_ENT) = NULL;                         \
        PgEnt_pages_in_use(FROM_PG_ENT) = PgEnt_strs_in_use(FROM_PG_ENT) = 0


#define DETACH_PAGES(_PG_ENT)                                                              \
  LOCK(PgEnt_lock(GLOBAL##_PG_ENT));					                   \
  if (PgEnt_first(LOCAL##_PG_ENT))  {					                   \
    MOVE_PAGES(LOCAL##_PG_ENT, GLOBAL##_PG_ENT);			                   \
  } else {								                   \
    UPDATE_STATS(PgEnt_pages_in_use(GLOBAL##_PG_ENT), PgEnt_pages_in_use(LOCAL##_PG_ENT)); \
    UPDATE_STATS(PgEnt_strs_in_use(GLOBAL##_PG_ENT), PgEnt_strs_in_use(LOCAL##_PG_ENT));   \
    PgEnt_pages_in_use(LOCAL##_PG_ENT) = PgEnt_strs_in_use(LOCAL##_PG_ENT) = 0 ;           \
  }									                   \
  UNLOCK(PgEnt_lock(GLOBAL##_PG_ENT))
      

#define ATTACH_PAGES(_PG_ENT)                                                              \
        if (PgEnt_first(GLOBAL##_PG_ENT)) {                                                \
          MOVE_PAGES(GLOBAL##_PG_ENT, LOCAL##_PG_ENT);                                     \
        }

/*******************************************************************************************
#define GET_PAGE_FIRST_LEVEL(PG_HD)       GET_VOID_PAGE(PG_HD)
#define GET_VOID_PAGE_NEXT_LEVEL(PG_HD)   GET_ALLOC_PAGE(PG_HD)
#define GET_ALLOC_PAGE_NEXT_LEVEL(PG_HD)
*******************************************************************************************/
#define GET_PAGE_FIRST_LEVEL(PG_HD)       GET_ALLOC_PAGE(PG_HD)
#define GET_ALLOC_PAGE_NEXT_LEVEL(PG_HD)  GET_VOID_PAGE(PG_HD)
#define GET_VOID_PAGE_NEXT_LEVEL(PG_HD)

#define GET_ALLOC_PAGE(PG_HD)						                   \
        LOCK(PgEnt_lock(GLOBAL_pages_alloc));		                                   \
        if ((PG_HD = PgEnt_first(GLOBAL_pages_alloc)) == NULL) {                           \
          UNLOCK(PgEnt_lock(GLOBAL_pages_alloc));		                           \
          GET_ALLOC_PAGE_NEXT_LEVEL(PG_HD);				                   \
        } else {                                                                           \
          PgEnt_first(GLOBAL_pages_alloc) = (pg_hd_ptr)(((void *)PG_HD) + Yap_page_size);  \
          if (PgEnt_first(GLOBAL_pages_alloc) == PgEnt_last(GLOBAL_pages_alloc))           \
            PgEnt_first(GLOBAL_pages_alloc) = NULL;                                        \
          UNLOCK(PgEnt_lock(GLOBAL_pages_alloc));		                           \
        }

#define GET_VOID_PAGE(PG_HD)			                                           \
        LOCK(PgEnt_lock(GLOBAL_pages_void));		                                   \
        if ((PG_HD = PgEnt_first(GLOBAL_pages_void)) == NULL) {                            \
          UNLOCK(PgEnt_lock(GLOBAL_pages_void));		                           \
          GET_VOID_PAGE_NEXT_LEVEL(PG_HD);				                   \
        } else {                          				                   \
          if ((PgEnt_first(GLOBAL_pages_void) = PgHd_next(PG_HD)) == NULL)                 \
            PgEnt_last(GLOBAL_pages_void) = NULL;                                          \
          UNLOCK(PgEnt_lock(GLOBAL_pages_void));		                           \
        }

#define PUT_PAGE(PG_HD, PG_ENT)	                                                           \
        if ((PgHd_next(PG_HD) = PgEnt_first(PG_ENT)) == NULL)                              \
          PgEnt_last(PG_ENT) = PG_HD;                                                      \
        else                                                                               \
          PgHd_previous(PgHd_next(PG_HD)) = PG_HD;                                         \
        PgEnt_first(PG_ENT) = PG_HD;                                                       \
        UPDATE_STATS(PgEnt_pages_in_use(PG_ENT), 1)

#define PUT_VOID_PAGE(PG_HD, PG_ENT)	                                                   \
        if ((PgHd_next(PG_HD) = PgEnt_first(PG_ENT)) == NULL)                              \
          PgEnt_last(PG_ENT) = PG_HD;                                                      \
        PgEnt_first(PG_ENT) = PG_HD

#ifdef THREADS
#define GET_FREE_PAGE(PG_HD)						                   \
        if ((PG_HD = PgEnt_first(LOCAL_pages_void)) == NULL) {                             \
	  GET_PAGE_FIRST_LEVEL(PG_HD);	                                                   \
        } else {                          				                   \
          if ((PgEnt_first(LOCAL_pages_void) = PgHd_next(PG_HD)) == NULL)                  \
            PgEnt_last(LOCAL_pages_void) = NULL;                                           \
        }
#define PUT_FREE_PAGE(PG_HD)					                           \
        PUT_VOID_PAGE(PG_HD, LOCAL_pages_void)
#else
#define GET_FREE_PAGE(PG_HD)                                                               \
        GET_PAGE_FIRST_LEVEL(PG_HD)
#define PUT_FREE_PAGE(PG_HD)			                                           \
	PUT_VOID_PAGE(PG_HD, GLOBAL_pages_void)
#endif

#define INIT_PAGE(PG_HD, STR_TYPE, PG_ENT)			     	                   \
        PgHd_strs_in_use(PG_HD) = 0;                                                       \
        PgHd_previous(PG_HD) = NULL;                                                       \
        PgHd_next(PG_HD) = NULL;                                                           \
        PgHd_first_str(PG_HD) = NULL;                                                      \
        PgHd_alloc_area(PG_HD) = (void *) (PG_HD + 1);                                     \
        PgHd_alloc_area(PG_HD) += sizeof(STR_TYPE) * PgEnt_strs_per_page(PG_ENT)

/*******************************************************************************************
#define OLD_INIT_PAGE(PG_HD, STR_TYPE, PG_ENT)				                   \
        { int i;                                                                           \
          STR_TYPE *aux_str;                                                               \
          PgHd_strs_in_use(PG_HD) = 0;                                                     \
          PgHd_previous(PG_HD) = NULL;                                                     \
          PgHd_next(PG_HD) = NULL;                                                         \
          PgHd_alloc_area(PG_HD) = NULL;                                                   \
          PgHd_first_str(PG_HD) = (void *) (PG_HD + 1);                                    \
          aux_str = (STR_TYPE *) PgHd_first_str(PG_HD);                                    \
          for (i = 1; i < PgEnt_strs_per_page(PG_ENT); i++) {                              \
            STRUCT_NEXT(aux_str) = aux_str + 1;                                            \
            aux_str++;                                                                     \
          }                                                                                \
          STRUCT_NEXT(aux_str) = NULL;                                                     \
        }
*******************************************************************************************/

#define ALLOC_SPACE()	          		                                           \
        LOCK(PgEnt_lock(GLOBAL_pages_alloc));			                           \
        if (PgEnt_first(GLOBAL_pages_alloc) == NULL) {                                     \
          int shmid;	                                                                   \
          void *mem_block;                                                                 \
          if ((shmid = shmget(IPC_PRIVATE, SHMMAX, SHM_R|SHM_W)) == -1)                    \
            Yap_Error(FATAL_ERROR, TermNil, "shmget error (ALLOC_PAGE)");                  \
          if ((mem_block = shmat(shmid, NULL, 0)) == (void *) -1)                          \
            Yap_Error(FATAL_ERROR, TermNil, "shmat error (ALLOC_PAGE)");                   \
          if (shmctl(shmid, IPC_RMID, 0) != 0)                                             \
            Yap_Error(FATAL_ERROR, TermNil, "shmctl error (ALLOC_PAGE)");                  \
          PgEnt_first(GLOBAL_pages_alloc) = (pg_hd_ptr)(mem_block + Yap_page_size);        \
          PgEnt_last(GLOBAL_pages_alloc) = (pg_hd_ptr)(mem_block + SHMMAX);                \
          UPDATE_STATS(PgEnt_pages_in_use(GLOBAL_pages_alloc), SHMMAX / Yap_page_size);    \
        }                                                                                  \
        UNLOCK(PgEnt_lock(GLOBAL_pages_alloc))


#ifdef LIMIT_TABLING
#define RECOVER_ALLOC_SPACE(PG_ENT, EXTRA_PG_ENT)					   \
        if (GLOBAL_max_pages == PgEnt_pages_in_use(GLOBAL_pages_alloc)) {                  \
          sg_fr_ptr sg_fr = GLOBAL_check_sg_fr;                                            \
          do {                                                                             \
            if (sg_fr)                                                                     \
              sg_fr = SgFr_next(sg_fr);                                                    \
            else                                                                           \
              sg_fr = GLOBAL_first_sg_fr;                                                  \
            if (sg_fr == NULL)                                                             \
              Yap_Error(FATAL_ERROR, TermNil, "no space left (RECOVER_SPACE)");            \
              /* see function 'InteractSIGINT' in file 'sysbits.c' */                      \
              /*   Yap_Error(PURE_ABORT, TermNil, "");             */                      \
              /*   restore_absmi_regs(&Yap_standard_regs);         */                      \
              /*   siglongjmp (LOCAL_RestartEnv, 1);               */                      \
            if (SgFr_first_answer(sg_fr) &&                                                \
                SgFr_first_answer(sg_fr) != SgFr_answer_trie(sg_fr)) {                     \
              SgFr_state(sg_fr) = ready;                                                   \
	      /* to remove free_answer_hash_chain with ANSWER_TRIE_LOCK_AT_ATOMIC_LEVEL_V04 */ \
	      free_answer_hash_chain(SgFr_hash_chain(sg_fr) PASS_REGS);                    \
	      SgFr_hash_chain(sg_fr) = NULL;                                               \
	      SgFr_first_answer(sg_fr) = NULL;                                             \
              SgFr_last_answer(sg_fr) = NULL;                                              \
              free_answer_trie(TrNode_child(SgFr_answer_trie(sg_fr)),                      \
                               TRAVERSE_MODE_NORMAL, TRAVERSE_POSITION_FIRST PASS_REGS);   \
              TrNode_child(SgFr_answer_trie(sg_fr)) = NULL;                                \
	    }                                                                              \
          } while (PgEnt_first(GLOBAL_pages_void) == PgEnt_first(PG_ENT));                 \
          GLOBAL_check_sg_fr = sg_fr;                                                      \
        } else {						                           \
	  ALLOC_SPACE();                                                                   \
        }



#elif THREADS
#define RECOVER_ALLOC_SPACE(PG_ENT, EXTRA_PG_ENT)					   \
        LOCK(PgEnt_lock(EXTRA_PG_ENT));    		                                   \
        if (PgEnt_first(EXTRA_PG_ENT)) {				                   \
          MOVE_PAGES(EXTRA_PG_ENT, PG_ENT);                                                \
          UNLOCK(PgEnt_lock(EXTRA_PG_ENT));		                                   \
        } else {                                                                           \
          UNLOCK(PgEnt_lock(EXTRA_PG_ENT));		                                   \
          ALLOC_SPACE();                                                                   \
 	}
#else
#define RECOVER_ALLOC_SPACE(PG_ENT, EXTRA_PG_ENT)					   \
        ALLOC_SPACE()
#endif

#define TEST_GET_FREE_PAGE(PG_HD, STR_TYPE, PG_ENT, EXTRA_PG_ENT)                          \
        while (PG_HD == NULL) {                                                            \
          UNLOCK_PAGE_ENTRY(PG_ENT);                                                       \
          GET_FREE_PAGE(PG_HD);	                                       			   \
          if (PG_HD) {                                                                     \
            INIT_PAGE(PG_HD, STR_TYPE, PG_ENT);				                   \
            LOCK_PAGE_ENTRY(PG_ENT);                                                       \
	    PUT_PAGE(PG_HD, PG_ENT);                                                       \
	  } else {                                                                         \
            RECOVER_ALLOC_SPACE(PG_ENT, EXTRA_PG_ENT);                                     \
            LOCK_PAGE_ENTRY(PG_ENT);                                                       \
            PG_HD = PgEnt_first(PG_ENT);                                                   \
          }                                                                                \
        }

#define GET_FREE_STRUCT(STR, STR_TYPE, PG_ENT, EXTRA_PG_ENT)				   \
        { pg_hd_ptr pg_hd;                                                                 \
          LOCK_PAGE_ENTRY(PG_ENT);                                                         \
          pg_hd = PgEnt_first(PG_ENT);                                                     \
          TEST_GET_FREE_PAGE(pg_hd, STR_TYPE, PG_ENT, EXTRA_PG_ENT);                       \
          if (PgHd_alloc_area(pg_hd)) {                                                    \
	    STR = ((STR_TYPE *) PgHd_alloc_area(pg_hd)) - 1;                               \
            if (STR == (STR_TYPE *) (pg_hd + 1))                                           \
              PgHd_alloc_area(pg_hd) = NULL;                                               \
            else                                                                           \
              PgHd_alloc_area(pg_hd) = (void *) STR;                                       \
          } else {                                                                         \
            STR = (STR_TYPE *) PgHd_first_str(pg_hd);                                      \
            PgHd_first_str(pg_hd) = (void *) STRUCT_NEXT(STR);                             \
          }                                                                                \
          if (PgHd_alloc_area(pg_hd) == NULL && PgHd_first_str(pg_hd) == NULL) {           \
            if ((PgEnt_first(PG_ENT) = PgHd_next(pg_hd)) == NULL)                          \
              PgEnt_last(PG_ENT) = NULL;                                                   \
            else                                                                           \
              PgHd_previous(PgHd_next(pg_hd)) = NULL;                                      \
          }                                                                                \
          UPDATE_STATS(PgHd_strs_in_use(pg_hd), 1);                                        \
          UPDATE_STATS(PgEnt_strs_in_use(PG_ENT), 1);                                      \
          UNLOCK_PAGE_ENTRY(PG_ENT);                                                       \
	}

/*******************************************************************************************
#define OLD_GET_FREE_STRUCT(STR, STR_TYPE, PG_ENT, EXTRA_PG_ENT)                           \
        { pg_hd_ptr pg_hd;                                                                 \
          LOCK_PAGE_ENTRY(PG_ENT);                                                         \
          pg_hd = PgEnt_first(PG_ENT);                                                     \
          TEST_GET_FREE_PAGE(pg_hd, STR_TYPE, PG_ENT, EXTRA_PG_ENT);                       \
          STR = (STR_TYPE *) PgHd_first_str(pg_hd);                                        \
          if ((PgHd_first_str(pg_hd) = (void *) STRUCT_NEXT(STR)) == NULL) {               \
            if ((PgEnt_first(PG_ENT) = PgHd_next(pg_hd)) == NULL)                          \
              PgEnt_last(PG_ENT) = NULL;                                                   \
            else                                                                           \
              PgHd_previous(PgHd_next(pg_hd)) = NULL;                                      \
          }                                                                                \
          UPDATE_STATS(PgHd_strs_in_use(pg_hd), 1);                                        \
          UPDATE_STATS(PgEnt_strs_in_use(PG_ENT), 1);                                      \
          UNLOCK_PAGE_ENTRY(PG_ENT);                                                       \
	}
*******************************************************************************************/

#define GET_NEXT_FREE_STRUCT(LOCAL_STR, STR, STR_TYPE, PG_ENT)                             \
        STR = LOCAL_STR;		                                                   \
        if (STR == NULL) {                                                                 \
          pg_hd_ptr pg_hd;                                                                 \
          LOCK_PAGE_ENTRY(PG_ENT);                                                         \
          pg_hd = PgEnt_first(PG_ENT);                                                     \
          TEST_GET_FREE_PAGE(pg_hd, STR_TYPE, PG_ENT, ___NOT_USED___);                     \
          STR = (STR_TYPE *) PgHd_first_str(pg_hd);                                        \
          PgHd_first_str(pg_hd) = NULL;                                                    \
          PgHd_strs_in_use(pg_hd) = PgEnt_strs_per_page(PG_ENT);                           \
          if ((PgEnt_first(PG_ENT) = PgHd_next(pg_hd)) == NULL)                            \
            PgEnt_last(PG_ENT) = NULL;                                                     \
          else                                                                             \
            PgHd_previous(PgHd_next(pg_hd)) = NULL;                                        \
          UPDATE_STATS(PgEnt_strs_in_use(PG_ENT), -PgHd_strs_in_use(pg_hd));               \
          UPDATE_STATS(PgEnt_strs_in_use(PG_ENT), PgEnt_strs_per_page(PG_ENT));            \
          UNLOCK_PAGE_ENTRY(PG_ENT);                                                       \
	}                                                                                  \
        LOCAL_STR = STRUCT_NEXT(STR)

#define PUT_FREE_STRUCT(STR, STR_TYPE, PG_ENT)                                             \
        { pg_hd_ptr pg_hd;                                                                 \
          pg_hd = PAGE_HEADER(STR);                                                        \
          LOCK_PAGE_ENTRY(PG_ENT);                                                         \
          UPDATE_STATS(PgEnt_strs_in_use(PG_ENT), -1);                                     \
          if (--PgHd_strs_in_use(pg_hd) == 0) {                                            \
            UPDATE_STATS(PgEnt_pages_in_use(PG_ENT), -1);                                  \
            if (PgHd_previous(pg_hd)) {                                                    \
              if ((PgHd_next(PgHd_previous(pg_hd)) = PgHd_next(pg_hd)) == NULL)            \
                PgEnt_last(PG_ENT) = PgHd_previous(pg_hd);                                 \
              else                                                                         \
                PgHd_previous(PgHd_next(pg_hd)) = PgHd_previous(pg_hd);                    \
	    } else {                                                                       \
              if ((PgEnt_first(PG_ENT) = PgHd_next(pg_hd)) == NULL)                        \
                PgEnt_last(PG_ENT) = NULL;                                                 \
              else                                                                         \
                PgHd_previous(PgHd_next(pg_hd)) = NULL;                                    \
	    }                                                                              \
            UNLOCK_PAGE_ENTRY(PG_ENT);                                                     \
            LOCK_PAGE_ENTRY(GLOBAL_pages_void);                                            \
            PUT_FREE_PAGE(pg_hd);	                    				   \
            UNLOCK_PAGE_ENTRY(GLOBAL_pages_void);                                          \
	  } else {                                                                         \
            STRUCT_NEXT(STR) = (STR_TYPE *) PgHd_first_str(pg_hd);                         \
            if (PgHd_alloc_area(pg_hd) == NULL && PgHd_first_str(pg_hd) == NULL) {         \
              PgHd_next(pg_hd) = NULL;                                                     \
              if ((PgHd_previous(pg_hd) = PgEnt_last(PG_ENT)) != NULL)                     \
                PgHd_next(PgHd_previous(pg_hd)) = pg_hd;                                   \
              PgEnt_last(PG_ENT) = pg_hd;                                                  \
            }                                                                              \
            PgHd_first_str(pg_hd) = (void *) STR;                                          \
            UNLOCK_PAGE_ENTRY(PG_ENT);                                                     \
          }                                                                                \
        }
#endif /***********************************************************************************/

#ifdef THREADS
#define ALLOC_STRUCT(STR, STR_TYPE, _PG_ENT)                                  \
        { GET_FREE_STRUCT(STR, STR_TYPE, LOCAL##_PG_ENT, GLOBAL##_PG_ENT); }

#define FREE_STRUCT(STR, STR_TYPE, _PG_ENT)                                   \
        { PUT_FREE_STRUCT(STR, STR_TYPE, LOCAL##_PG_ENT); }
#else
#define ALLOC_STRUCT(STR, STR_TYPE, _PG_ENT)                                  \
        { GET_FREE_STRUCT(STR, STR_TYPE, GLOBAL##_PG_ENT, ___NOT_USED___); }
#define FREE_STRUCT(STR, STR_TYPE, _PG_ENT)                                   \
        { PUT_FREE_STRUCT(STR, STR_TYPE, GLOBAL##_PG_ENT); }
#endif
#define ALLOC_NEXT_STRUCT(LOCAL_STR, STR, STR_TYPE, _PG_ENT)                  \
        { GET_NEXT_FREE_STRUCT(LOCAL_STR, STR, STR_TYPE, GLOBAL##_PG_ENT); }

#define ALLOC_TABLE_ENTRY(STR)         ALLOC_STRUCT(STR, struct table_entry, _pages_tab_ent)
#define FREE_TABLE_ENTRY(STR)           FREE_STRUCT(STR, struct table_entry, _pages_tab_ent)

#define ALLOC_SUBGOAL_ENTRY(STR)       ALLOC_STRUCT(STR, struct subgoal_entry, _pages_sg_ent)
#define FREE_SUBGOAL_ENTRY(STR)         FREE_STRUCT(STR, struct subgoal_entry, _pages_sg_ent)

#define ALLOC_SUBGOAL_FRAME(STR)       ALLOC_STRUCT(STR, struct subgoal_frame, _pages_sg_fr)
#define FREE_SUBGOAL_FRAME(STR)         FREE_STRUCT(STR, struct subgoal_frame, _pages_sg_fr)

#define ALLOC_DEPENDENCY_FRAME(STR)    ALLOC_STRUCT(STR, struct dependency_frame, _pages_dep_fr)
#define FREE_DEPENDENCY_FRAME(STR)      FREE_STRUCT(STR, struct dependency_frame, _pages_dep_fr)

#define ALLOC_SUBGOAL_TRIE_NODE(STR)   ALLOC_STRUCT(STR, struct subgoal_trie_node, _pages_sg_node)
#define FREE_SUBGOAL_TRIE_NODE(STR)    FREE_STRUCT(STR, struct subgoal_trie_node, _pages_sg_node)


#define ALLOC_SUBGOAL_TRIE_HASH(STR)   ALLOC_STRUCT(STR, struct subgoal_trie_hash, _pages_sg_hash)
#define FREE_SUBGOAL_TRIE_HASH(STR)     FREE_STRUCT(STR, struct subgoal_trie_hash, _pages_sg_hash)

#ifdef YAPOR
#define ALLOC_ANSWER_TRIE_NODE(STR)    ALLOC_NEXT_STRUCT(LOCAL_next_free_ans_node, STR, struct answer_trie_node, _pages_ans_node)
#else
#define ALLOC_ANSWER_TRIE_NODE(STR)    ALLOC_STRUCT(STR, struct answer_trie_node, _pages_ans_node)
#endif
#define FREE_ANSWER_TRIE_NODE(STR)      FREE_STRUCT(STR, struct answer_trie_node, _pages_ans_node)


#define ALLOC_ANSWER_TRIE_HASH(STR)    ALLOC_STRUCT(STR, struct answer_trie_hash, _pages_ans_hash)
#define FREE_ANSWER_TRIE_HASH(STR)      FREE_STRUCT(STR, struct answer_trie_hash, _pages_ans_hash)

#define V04_ALLOC_THB(STR)						\
  union trie_hash_buckets *aux;						\
  ALLOC_STRUCT(aux, union trie_hash_buckets, _pages_trie_hash_buckets); \
  STR = aux->hash_buckets


#define V04_FREE_BUCKETS(STR)               FREE_STRUCT((union trie_hash_buckets*)STR, union trie_hash_buckets, _pages_trie_hash_buckets)

#define V04_FREE_THB(STR)               FREE_STRUCT((union trie_hash_buckets*)STR, union trie_hash_buckets, _pages_trie_hash_buckets)


#define ALLOC_ANSWER_REF_NODE(STR)     ALLOC_STRUCT(STR, struct answer_ref_node, _pages_ans_ref_node)
#define FREE_ANSWER_REF_NODE(STR)      FREE_STRUCT(STR, struct answer_ref_node, _pages_ans_ref_node)

#define ALLOC_GLOBAL_TRIE_NODE(STR)    ALLOC_STRUCT(STR, struct global_trie_node, _pages_gt_node)
#define FREE_GLOBAL_TRIE_NODE(STR)      FREE_STRUCT(STR, struct global_trie_node, _pages_gt_node)

#define ALLOC_GLOBAL_TRIE_HASH(STR)    ALLOC_STRUCT(STR, struct global_trie_hash, _pages_gt_hash)
#define FREE_GLOBAL_TRIE_HASH(STR)      FREE_STRUCT(STR, struct global_trie_hash, _pages_gt_hash)

#define ALLOC_OR_FRAME(STR)            ALLOC_STRUCT(STR, struct or_frame, _pages_or_fr)
#define FREE_OR_FRAME(STR)              FREE_STRUCT(STR, struct or_frame, _pages_or_fr)

#define ALLOC_QG_SOLUTION_FRAME(STR)   ALLOC_STRUCT(STR, struct query_goal_solution_frame, _pages_qg_sol_fr)
#define FREE_QG_SOLUTION_FRAME(STR)     FREE_STRUCT(STR, struct query_goal_solution_frame, _pages_qg_sol_fr)

#define ALLOC_QG_ANSWER_FRAME(STR)     ALLOC_STRUCT(STR, struct query_goal_answer_frame, _pages_qg_ans_fr)
#define FREE_QG_ANSWER_FRAME(STR)       FREE_STRUCT(STR, struct query_goal_answer_frame, _pages_qg_ans_fr)

#define ALLOC_SUSPENSION_FRAME(STR)    ALLOC_STRUCT(STR, struct suspension_frame, _pages_susp_fr)
#define FREE_SUSPENSION_FRAME(STR)      FREE_BLOCK(SuspFr_global_start(STR));                          \
                                        FREE_STRUCT(STR, struct suspension_frame, _pages_susp_fr)

#define ALLOC_TG_SOLUTION_FRAME(STR)   ALLOC_STRUCT(STR, struct table_subgoal_solution_frame, _pages_tg_sol_fr)
#define FREE_TG_SOLUTION_FRAME(STR)     FREE_STRUCT(STR, struct table_subgoal_solution_frame, _pages_tg_sol_fr)

#define ALLOC_TG_ANSWER_FRAME(STR)     ALLOC_STRUCT(STR, struct table_subgoal_answer_frame, _pages_tg_ans_fr)
#define FREE_TG_ANSWER_FRAME(STR)       FREE_STRUCT(STR, struct table_subgoal_answer_frame, _pages_tg_ans_fr)


#ifdef THREADS_SUBGOAL_SHARING_WITH_PAGES_SG_FR_ARRAY
#define ALLOC_SG_FR_ARRAY(BUCKET_PTR, NUM_BUCKETS)			   \
  {  ALLOC_STRUCT(BUCKET_PTR, struct sg_fr_bkt_array, _pages_sg_fr_array); \
     INIT_BUCKETS(BUCKET_PTR, NUM_BUCKETS); 	                           \
  }

#define FREE_SG_FR_ARRAY(STR)  FREE_STRUCT((sg_fr_bkt_array_ptr) STR, struct sg_fr_bkt_array, _pages_sg_fr_array)
#else
#define ALLOC_SG_FR_ARRAY(BUCKET_PTR, NUM_BUCKETS)   ALLOC_BUCKETS(BUCKET_PTR, NUM_BUCKETS)
#define FREE_SG_FR_ARRAY(BUCKET_PTR)                 FREE_BUCKETS(BUCKET_PTR)
#endif /* THREADS_SUBGOAL_SHARING_WITH_SG_FR_ARRAY */


#define VAL_CAS(PTR, OLD, NEW)                 __sync_val_compare_and_swap((PTR), (OLD), (NEW))
#define BOOL_CAS(PTR, OLD, NEW)                __sync_bool_compare_and_swap((PTR), (OLD), (NEW))
#define atomic_inc(P)                          __sync_add_and_fetch((P), 1)


//#define BOOL_CAS(PTR, OLD, NEW)                __sync_lock_test_and_set((PTR), (OLD), (NEW))
// __sync_lock_test_and_set

//#define BOOL_CAS(PTR, OLD, NEW)                (*(PTR) == (OLD) ? __sync_bool_compare_and_swap((PTR), (OLD), (NEW)) : 0)


#define ALLOC_TRIE_HASH_BUCKETS(PTR, STR_HASH_BKTS)   ALLOC_BLOCK(PTR, sizeof(STR_HASH_BKTS), STR_HASH_BKTS)
#define FREE_TRIE_HASH_BUCKETS(PTR)                   FREE_BLOCK(PTR)

#define	FREE_EXPANSION_NODES(EXP_NODES, STR_PTR)     \
  do {                                               \
    STR_PTR next_exp_nodes = TrNode_next(EXP_NODES); \
    FREE_ANSWER_TRIE_NODE(EXP_NODES);                \
    EXP_NODES = next_exp_nodes;                      \
  } while(EXP_NODES)


#if defined(ANSWER_TRIE_LOCK_AT_ATOMIC_LEVEL_V01) || defined(SUBGOAL_TRIE_LOCK_AT_ATOMIC_LEVEL_V01)

#define READ_BUCKET_PTR(BUCKET)                ((CELL)(*BUCKET) & ~(CELL)0x1)
#define CLOSE_BUCKET(BUCKET)                   ((CELL)(BUCKET) | (CELL)0x1)
#define NEW_HASH_REF(BUCKET, NEW_HASH, STR)    ((*BUCKET) = (STR *) ((CELL)(NEW_HASH) | (CELL)0x3))
#define IS_NEW_HASH_REF(BUCKET)                ((CELL)(BUCKET) & (CELL)0x2)
#define CLOSE_HASH(HASH_NUM_NODES)             ((HASH_NUM_NODES << 1) | (int) 1)
#define OPEN_HASH(HASH)                        __sync_add_and_fetch(&(Hash_num_nodes(HASH)), (int)-1)
#define Inc_HashNode_num_nodes(HASH)           __sync_add_and_fetch(&(Hash_num_nodes(HASH)), (int)2)
#endif

#if defined(ANSWER_TRIE_LOCK_AT_ATOMIC_LEVEL_V02) || defined(SUBGOAL_TRIE_LOCK_AT_ATOMIC_LEVEL_V02)

#define IS_NEW_HASH_REF_V02(BUCKET)          ((CELL)(BUCKET) & (CELL)0x1)
#define OPEN_HASH_V02(HASH, EXP_NODES)       (Hash_exp_nodes(HASH) = EXP_NODES)
#define Inc_HashNode_num_nodes_v02(HASH)      __sync_add_and_fetch(&(Hash_num_nodes(HASH)), (int)1)
#endif 

#if defined(ANSWER_TRIE_LOCK_AT_ATOMIC_LEVEL_V03) || defined(SUBGOAL_TRIE_LOCK_AT_ATOMIC_LEVEL_V03)

#define IS_NEW_HASH_REF_V03(BUCKET)               ((CELL)(BUCKET) & (CELL)0x1)
#define IS_CLOSED_HASH_V03(HASH)                  ((CELL)(HASH) & (CELL)0x1)
#define CLOSE_HASH_V03(HASH)                      ((ans_hash_bkts_ptr)((CELL)HASH | (CELL)0x1))
#define CLOSE_SG_HASH_V03(HASH)                   ((sg_hash_bkts_ptr)((CELL)HASH | (CELL)0x1))

#define Inc_HashNode_num_nodes_v03(HASH)          __sync_add_and_fetch(&(Hash_num_nodes(HASH)), (int)1)
#define Add_HashNode_num_nodes_v03(HASH, VALUE)   __sync_add_and_fetch(&(Hash_num_nodes(HASH)), (int)VALUE)
#define OPEN_HASH_V03(HASH)                      (AnsHash_hash_bkts(HASH) = (ans_hash_bkts_ptr)((CELL) AnsHash_hash_bkts(HASH) & ~(CELL)0x1))
#define OPEN_SG_HASH_V03(HASH)                   (SgHash_hash_bkts(HASH) = (sg_hash_bkts_ptr)((CELL) SgHash_hash_bkts(HASH) & ~(CELL)0x1))
#endif 

#if defined(ANSWER_TRIE_LOCK_AT_ATOMIC_LEVEL_V04) ||  defined(SUBGOAL_TRIE_LOCK_AT_ATOMIC_LEVEL_V04)

#define V04_INIT_BUCKETS(BUCKET_PTR, PREV_HASH)                       \
  { int i; void **init_bucket_ptr;                                    \
  *BUCKET_PTR++ = (void *) (PREV_HASH);                               \
  init_bucket_ptr = (void **) BUCKET_PTR;                             \
  for (i = BASE_HASH_BUCKETS; i != 0; i--)                            \
    *init_bucket_ptr++ = (void *) V04_TAG(BUCKET_PTR);                \
  }

#if defined(ANSWER_TRIE_LOCK_AT_ATOMIC_LEVEL_V04_BUFFER_ALLOC)  || defined(SUBGOAL_TRIE_LOCK_AT_ATOMIC_LEVEL_V04_BUFFER_ALLOC)

#define V04_ALLOC_BUCKETS(BUCKET_PTR, PREV_HASH, STR)	   		                \
  { void **alloc_bucket_ptr;						                \
    if(LOCAL_trie_buckets_buffer == NULL) {				                \
      V04_ALLOC_THB(alloc_bucket_ptr);					                \
      V04_INIT_BUCKETS(alloc_bucket_ptr, PREV_HASH);			                \
    } else {								                \
      alloc_bucket_ptr = LOCAL_trie_buckets_buffer;		                        \
      LOCAL_trie_buckets_buffer = NULL;				                        \
      *alloc_bucket_ptr++ = (void *) (PREV_HASH);			                \
    }									                \
    BUCKET_PTR = (STR **) alloc_bucket_ptr;	                	                \
  }

#define V04_FREE_TRIE_HASH_BUCKETS(PTR, BKT, STR)		               \
  { V04_SET_HASH_BUCKET(BKT, PTR, STR);					       \
    LOCAL_trie_buckets_buffer = (((void**)V04_UNTAG(PTR)) - 1);                \
  }

#define V04_FREE_BUCKET_ARRAY(PTR)				\
  V04_FREE_BUCKETS(((union trie_hash_buckets *)(((long)V04_UNTAG(PTR)) - sizeof(ans_node_ptr *))));

#else /* !ANSWER_TRIE_LOCK_AT_ATOMIC_LEVEL_V04_MIGS_ALLOC */

#define V04_ALLOC_BUCKETS(BUCKET_PTR, PREV_HASH, STR)	                             \
  { void **alloc_bucket_ptr;						             \
    V04_ALLOC_THB(alloc_bucket_ptr);					             \
    V04_INIT_BUCKETS(alloc_bucket_ptr, PREV_HASH);                                   \
    BUCKET_PTR = (STR **) alloc_bucket_ptr;                                          \
  }

#define V04_FREE_TRIE_HASH_BUCKETS(PTR, BKT, STR)				     \
   V04_FREE_THB(((STR *) V04_UNTAG(PTR)) - 1) /* DOES NOT DO ANYTHING */
  /*  FREE_BLOCK(((ans_node_ptr *) V04_UNTAG(STR)) - 1) */


#define V04_FREE_BUCKET_ARRAY(PTR)				\
  V04_FREE_BUCKETS(((union trie_hash_buckets *)(((long)V04_UNTAG(PTR)) - sizeof(ans_node_ptr *))));



#endif /* ANSWER_TRIE_LOCK_AT_ATOMIC_LEVEL_V04_MIGS_ALLOC */
#endif /* ANSWER_TRIE_LOCK_AT_ATOMIC_LEVEL_V04 */


#if defined(THREADS_FULL_SHARING_FTNA_3) 

#define FTNA_3_ALLOC_THB(STR)						               \
  union consumer_trie_hash_buckets *aux;					       \
  ALLOC_STRUCT(aux, union consumer_trie_hash_buckets, _pages_cons_trie_hash_buckets);  \
  STR = aux->hash_buckets

#define FTNA_3_INIT_BUCKETS(BUCKET_PTR)                               \
  { int i; void **init_bucket_ptr;                                    \
  init_bucket_ptr = (void **) BUCKET_PTR;                             \
  for (i = BASE_HASH_BUCKETS; i != 0; i--)                            \
    *init_bucket_ptr++ = (void *) NULL;                               \
  }

#define FTNA_3_ALLOC_BUCKETS(BUCKET_PTR, STR)	                                     \
  { void **alloc_bucket_ptr;						             \
    FTNA_3_ALLOC_THB(alloc_bucket_ptr);					             \
    FTNA_3_INIT_BUCKETS(alloc_bucket_ptr);                                           \
    BUCKET_PTR = (STR **) alloc_bucket_ptr;                                          \
  }

#define FTNA_3_FREE_TRIE_HASH_BUCKETS(STR)  FREE_STRUCT(((union consumer_trie_hash_buckets *) STR), union consumer_trie_hash_buckets, _pages_cons_trie_hash_buckets)

#endif

/************************************************************************
**                         Bitmap manipulation                         **
************************************************************************/

#define BITMAP_empty(b)		       ((b) == 0)
#define BITMAP_member(b,n)	       (((b) & (1<<(n))) != 0)
#define BITMAP_alone(b,n)	       ((b) == (1<<(n)))
#define BITMAP_subset(b1,b2)	       (((b1) & (b2)) == b2)
#define BITMAP_same(b1,b2)             ((b1) == (b2))
#define BITMAP_clear(b)	               ((b) = 0)
#define BITMAP_and(b1,b2)              ((b1) &= (b2))
#define BITMAP_minus(b1,b2)            ((b1) &= ~(b2))
#define BITMAP_insert(b,n)	       ((b) |= (1<<(n)))
#define BITMAP_delete(b,n)	       ((b) &= (~(1<<(n))))
#define BITMAP_copy(b1,b2)	       ((b1) = (b2))
#define BITMAP_intersection(b1,b2,b3)  ((b1) = ((b2) & (b3)))
#define BITMAP_difference(b1,b2,b3)    ((b1) = ((b2) & (~(b3))))



/************************************************************************
**                            Debug macros                             **
************************************************************************/

#define INFORMATION_MESSAGE(MESSAGE,ARGS...)                            \
        Sfprintf(Serror, "[ " MESSAGE " ]\n", ##ARGS)

#ifdef YAPOR
#define ERROR_MESSAGE(MESSAGE)                                          \
        Yap_Error(INTERNAL_ERROR, TermNil, "W%d - " MESSAGE, worker_id)
#else
#define ERROR_MESSAGE(MESSAGE)                                          \
        Yap_Error(INTERNAL_ERROR, TermNil, MESSAGE)
#endif /* YAPOR */

#ifdef DEBUG_TABLING
#define TABLING_ERROR_CHECKING(PROCEDURE,TEST)                          \
        if (TEST) ERROR_MESSAGE(#PROCEDURE ": " #TEST)
#else
#define TABLING_ERROR_CHECKING(PROCEDURE,TEST)
#endif /* DEBUG_TABLING */

#ifdef DEBUG_YAPOR
#define YAPOR_ERROR_CHECKING(PROCEDURE,TEST)                            \
        if (TEST) ERROR_MESSAGE(#PROCEDURE ": " #TEST)
#else
#define YAPOR_ERROR_CHECKING(PROCEDURE,TEST)
#endif /* DEBUG_YAPOR */

#ifdef DEBUG_OPTYAP
#define OPTYAP_ERROR_CHECKING(PROCEDURE,TEST)                           \
        if (TEST) ERROR_MESSAGE(#PROCEDURE ": " #TEST)
#else
#define OPTYAP_ERROR_CHECKING(PROCEDURE,TEST)
#endif /* DEBUG_OPTYAP */

#ifdef OUTPUT_THREADS_TABLING
#define INFO_THREADS(MESSAGE, ARGS...)                                  \
        fprintf(LOCAL_thread_output, "[ " MESSAGE " ]\n", ##ARGS)
#define INFO_THREADS_MAIN_THREAD(MESSAGE, ARGS...)                      \
        Sfprintf(Serror, "[ " MESSAGE " ]\n", ##ARGS)
#else
#define INFO_THREADS(MESG, ARGS...)
#define INFO_THREADS_MAIN_THREAD(MESSAGE, ARGS...)
#endif /* OUTPUT_THREADS_TABLING */
