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
* File:		load_none.c						 *
* comments:	dummy dynamic loaderr of external routines		 *
*************************************************************************/

#include "Yap.h"
#include "Foreign.h"

#ifdef NO_DYN

/*
 *   YAPFindExecutable(argv[0]) should be called on yap initialization to
 *   locate the executable of Yap
*/
void
YAPFindExecutable(char *name)
{
  /* signal name not found */
 strcpy(YapExecutable,"./yap");
}


/*
 * LoadForeign(file_name,proc_name,init_proc) dynamically loads a foreign
 * code file and locates an initialization routine
*/
Int
LoadForeign(StringList ofiles, StringList libs,
	    char *proc_name, YapInitProc *init_proc)
{
  strcpy(LoadMsg,"load_foreign not supported in this version of Yap");
  return LOAD_FAILLED;
}

void 
ShutdownLoadForeign(void)
{
}

Int
ReLoadForeign(StringList ofiles, StringList libs,
	       char *proc_name,	YapInitProc *init_proc)
{
  return(LoadForeign(ofiles,libs, proc_name, init_proc));
}

#endif

