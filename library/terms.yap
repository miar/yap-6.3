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
* File:		terms.yap						 *
* Last rev:	5/12/99							 *
* mods:									 *
* comments:	Term manipulation operations				 *
*									 *
*************************************************************************/

:- module(terms, [
	term_hash/2,
	term_hash/4,
	term_variables/2,
	variant/2,
	subsumes/2,
	subsumes_chk/2,
        cyclic_term/1,
        acyclic_term/1
    ]).

term_hash(T,H) :-
	term_hash(T, -1, 33554432, H).

subsumes_chk(X,Y) :-
	\+ \+ subsumes(X,Y).





