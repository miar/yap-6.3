
/*************************************************************************
*									 *
*	 YAP Prolog 							 *
*									 *
*	Yap Prolog was developed at NCCUP - Universidade do Porto	 *
*									 *
* Copyright L.Damas, V.S.Costa and Universidade do Porto 1985-2011	 *
*									 *
**************************************************************************
*									 *
* File:		qly.yap							 *
* Last rev:								 *
* mods:									 *
* comments:	fast save/restore					 *
*									 *
*************************************************************************/

qsave_program(File) :-
	'$save_program_status',
	open(File, write, S, [type(binary)]),
	'$qsave_program'(S),
	close(S).	

'$save_program_status' :-
	findall(F:V,'$x_yap_flag'(F,V),L),
	recordz('$program_state',L,_).

% there is some ordering between flags.
'$x_yap_flag'(language, V).
'$x_yap_flag'(X, V) :-
	yap_flag(X, V),
	X \= language,
	X \= timezone,
	X \= user_input,
	X \= user_output,
	X \= user_error,
	X \= version.

'$init_state' :-
	recorded('$program_state', _, _), !,
	'$do_init_state'.
'$init_state'.

'$do_init_state' :-
	compile_expressions,
	 '$init_preds',
	 fail.
'$do_init_state' :-
	recorded('$program_state',L,R),
	erase(R),
	lists:member(F:V,L),
	catch(yap_flag(F,V),_,fail),
	fail.
'$do_init_state' :-
	set_value('$user_module',user),
	'$protect',
	fail.
'$do_init_state' :-
	'$current_module'(prolog),
	module(user),
	fail.
'$do_init_state' :-
	'$init_from_saved_state_and_args',
	 fail.
'$do_init_state'.

%
% first, recover what we need from the saved state...
%
'$init_from_saved_state_and_args' :-
	'$init_path_extensions',
	fail.
% use if we come from a save_program and we have SWI's shlib
'$init_from_saved_state_and_args' :-
	recorded('$reload_foreign_libraries',G,R),
	erase(R),
	shlib:reload_foreign_libraries,
	fail.
% this should be done before -l kicks in.
'$init_from_saved_state_and_args' :-
	  '$access_yap_flags'(16,0),
	  ( exists('~/.yaprc') -> load_files('~/.yaprc', []) ; true ),
	  ( exists('~/.prologrc') -> load_files('~/.prologrc', []) ; true ),
	  ( exists('~/prolog.ini') -> load_files('~/prolog.ini', []) ; true ),
	  fail.
% use if we come from a save_program and we have a goal to execute
'$init_from_saved_state_and_args' :-
	get_value('$consult_on_boot',X), X \= [],
	set_value('$consult_on_boot',[]),
	'$do_startup_reconsult'(X),
	fail.
'$init_from_saved_state_and_args' :-
	'$startup_goals',
	fail.
'$init_from_saved_state_and_args' :-
	recorded('$restore_goal',G,R),
	erase(R),
	prompt(_,'| '),
	'$system_catch'('$do_yes_no'((G->true),user),user,Error,user:'$Error'(Error)),
	fail.
'$init_from_saved_state_and_args'.

'$init_path_extensions' :-
	get_value('$extend_file_search_path',P), !,
	P \= [],
	set_value('$extend_file_search_path',[]),
	'$extend_file_search_path'(P).
'$init_path_extensions'.
 
% then we can execute the programs.
'$startup_goals' :-
	recorded('$startup_goal',G,_),
	'$current_module'(Module),
	'$system_catch'('$query'(once(G), []),Module,Error,user:'$Error'(Error)),
	fail.
'$startup_goals' :-
	get_value('$init_goal',GA),
	GA \= [],
	set_value('$init_goal',[]),
	'$run_atom_goal'(GA),
	fail.
'$startup_goals' :-
	get_value('$myddas_goal',GA), GA \= [],
	set_value('$myddas_goal',[]),
	get_value('$myddas_user',User), User \= [],
	set_value('$myddas_user',[]),
	get_value('$myddas_db',Db), Db \= [],
	set_value('$myddas_db',[]),
	get_value('$myddas_host',HostT),
	( HostT \= [] ->
	  Host = HostT,
	  set_value('$myddas_host',[])
	;
	  Host = localhost
	),
	get_value('$myddas_pass',PassT),
	( PassT \= [] ->
	  Pass = PassT,
	  set_value('$myddas_pass',[])
	;
	  Pass = ''
	),
	use_module(library(myddas)),
	call(db_open(mysql,myddas,Host/Db,User,Pass)),
	'$myddas_import_all',
	fail.
'$startup_goals'.

 %
 % MYDDAS: Import all the tables from one database
 %

 '$myddas_import_all':-
	 call(db_my_show_tables(myddas,table(Table))),
	 call(db_import(myddas,Table,Table)),
	 fail.
 '$myddas_import_all'.
	 



qsave_module(Mod) :-
	recorded('$module', '$module'(F,Mod,Exps), _),
	'$fetch_parents_module'(Mod, Parents),
	'$fetch_imports_module'(Mod, Imps),
	'$fetch_multi_files_module'(Mod, MFs),
	'$fetch_meta_predicates_module'(Mod, Metas),
	'$fetch_module_transparents_module'(Mod, ModTransps),
	asserta(Mod:'@mod_info'(F, Exps, Parents, Imps, Metas, ModTransps)),
	atom_concat(Mod,'.qly',OF),
	open(OF, write, S, [type(binary)]),
	'$qsave_module_preds'(S, Mod),
	close(S),
	abolish(Mod:'@mod_info'/6),
	fail.
qsave_module(_).

qload_program(File) :-
	open(File, read, S, [type(binary)]),
	'$qload_program'(S),
	close(S).

qload_module(Mod) :-
	atom_concat(Mod,'.qly',IF),
	open(IF, read, S, [type(binary)]),
	'$qload_module_preds'(S),
	close(S),
	fail.
qload_module(Mod) :-
	'$complete_read'(Mod).

'$complete_read'(Mod) :-
	retract(Mod:'@mod_info'(F, Exps, Parents, Imps, Metas, ModTransps)),
	abolish(Mod:'$mod_info'/6),
	recorda('$module', '$module'(F,Mod,Exps), _),
	'$install_parents_module'(Mod, Parents),
	'$install_imports_module'(Mod, Imps),
	'$install_multi_files_module'(Mod, MFs),
	'$install_meta_predicates_module'(Mod, Metas),
	'$install_module_transparents_module'(Mod, ModTransps).
	
'$fetch_imports_module'(Mod, Imports) :-
	findall(Info, '$fetch_import_module'(Mod, Info), Imports).

% detect an importerator that is local to the module.
'$fetch_import_module'(Mod, '$import'(Mod0,Mod,G0,G,N,K)) :-
	recorded('$import', '$import'(Mod0,Mod,G0,G,N,K), _).

'$fetch_parents_module'(Mod, Parents) :-
	findall(Parent, prolog:'$parent_module'(Mod,Parent), Parents).

'$fetch_module_transparents_module'(Mod, Module_Transparents) :-
	findall(Info, '$fetch_module_transparent_module'(Mod, Info), Module_Transparents).

% detect an module_transparenterator that is local to the module.
'$fetch_module_transparent_module'(Mod, '$module_transparent'(F,Mod,N,P)) :-
	prolog:'$module_transparent'(F,Mod0,N,P), Mod0 == Mod.

'$fetch_meta_predicates_module'(Mod, Meta_Predicates) :-
	findall(Info, '$fetch_meta_predicate_module'(Mod, Info), Meta_Predicates).

% detect an meta_predicateerator that is local to the module.
'$fetch_meta_predicate_module'(Mod, '$meta_predicate'(F,Mod,N,P)) :-
	prolog:'$meta_predicate'(F,Mod0,N,P), Mod0 == Mod.

'$fetch_multi_files_module'(Mod, Multi_Files) :-
	findall(Info, '$fetch_multi_file_module'(Mod, Info), Multi_Files).

% detect an multi_fileerator that is local to the module.
'$fetch_multi_file_module'(Mod, '$defined'(FileName,Name,Arity,Mod)) :-
	recorded('$multifile_defs','$defined'(FileName,Name,Arity,Mod), _).

'$fetch_term_expansions_module'(Mod, Term_Expansions) :-
	findall(Info, '$fetch_term_expansion_module'(Mod, Info), Term_Expansions).

% detect an term_expansionerator that is local to the module.
'$fetch_term_expansion_module'(Mod,'$defined'(FileName,Name,Arity,Mod)) :-
	recorded('$multifile_defs','$defined'(FileName,Name,Arity,Mod), _).



'$install_ops_module'(_, []).
'$install_ops_module'(Mod, op(X,Y,Op).Ops) :-
	op(X, Y, Mod:Op),
	'$install_ops_module'(Mod, Ops).

'$install_imports_module'(_, []).
'$install_imports_module'(Mod, Import.Imports) :-
	recordz('$import', Import, _),
	'$install_imports_module'(Mod, Imports).

'$install_parents_module'(_, []).
'$install_parents_module'(Mod, Parent.Parents) :-
	assert(prolog:Parent),
	'$install_parents_module'(Mod, Parents).

'$install_module_transparents_module'(_, []).
'$install_module_transparents_module'(Mod, Module_Transparent.Module_Transparents) :-
	assert(prolog:Module_Transparent),
	'$install_module_transparents_module'(Mod, Module_Transparents).

'$install_meta_predicates_module'(_, []).
'$install_meta_predicates_module'(Mod, Meta_Predicate.Meta_Predicates) :-
	assert(prolog:Meta_Predicate),
	'$install_meta_predicates_module'(Mod, Meta_Predicates).

'$install_multi_files_module'(_, []).
'$install_multi_files_module'(Mod, Multi_File.Multi_Files) :-
	recordz('$multifile_defs',Multi_File, _).
	'$install_multi_files_module'(Mod, Multi_Files).



