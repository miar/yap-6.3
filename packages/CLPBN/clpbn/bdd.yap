
/************************************************

  BDDs in CLP(BN)

A variable is represented by the N possible cases it can take

V = v(Va, Vb, Vc)

The generic formula is

V <- X, Y 

Va <- P*X1*Y1 + Q*X2*Y2 + ...


 
**************************************************/

:- module(clpbn_bdd,
          [bdd/3,
           set_solver_parameter/2,
           init_bdd_solver/4,
           run_bdd_solver/3,
           finalize_bdd_solver/1,
	   check_if_bdd_done/1
          ]).


:- use_module(library('clpbn/dists'),
          [dist/4,
           get_dist_domain/2,
           get_dist_domain_size/2,
           get_dist_all_sizes/2,
           get_dist_params/2
          ]).


:- use_module(library('clpbn/display'),
         [clpbn_bind_vals/3]).

:- use_module(library('clpbn/aggregates'),
	      [check_for_agg_vars/2]).


:- use_module(library(atts)).

:- use_module(library(hacks)).

:- use_module(library(lists)).

:- use_module(library(dgraphs)).

:- use_module(library(bdd)).

:- use_module(library(ddnnf)).

:- use_module(library(simpbool)).

:- use_module(library(rbtrees)).

:- use_module(library(bhash)).

:- use_module(library(matrix)).

:- dynamic network_counting/1.

:- attribute order/1.

:- dynamic bdds/1.
%bdds(ddnnf).
bdds(bdd).

check_if_bdd_done(_Var).

bdd([[]],_,_) :- !.
bdd([QueryVars], AllVars, AllDiffs) :-
	init_bdd_solver(_, AllVars, _, BayesNet),
	run_bdd_solver([QueryVars], LPs, BayesNet),
	finalize_bdd_solver(BayesNet),
	clpbn_bind_vals([QueryVars], [LPs], AllDiffs).

init_bdd_solver(_, AllVars0, _, bdd(Term, Leaves, Tops)) :-
%	check_for_agg_vars(AllVars0, AllVars1),
	AllVars0 = AllVars1,
	sort_vars(AllVars1, AllVars, Leaves),
	order_vars(AllVars, 0),
	rb_new(Vars0),
	rb_new(Pars0),
	init_tops(Leaves,Tops),
	get_vars_info(AllVars, Vars0, _Vars, Pars0, _Pars, Leaves, Tops, Term, []).

order_vars([], _).
order_vars([V|AllVars], I0) :-
	put_atts(V, [order(I0)]),
	I is I0+1,
	order_vars(AllVars, I).


init_tops([],[]).
init_tops(_.Leaves,_.Tops) :-
	init_tops(Leaves,Tops).

sort_vars(AllVars0, AllVars, Leaves) :-
	dgraph_new(Graph0),
	build_graph(AllVars0, Graph0, Graph),
	dgraph_leaves(Graph, Leaves),
	dgraph_top_sort(Graph, AllVars).

build_graph([], Graph, Graph).
build_graph(V.AllVars0, Graph0, Graph) :-
	clpbn:get_atts(V, [dist(_DistId, Parents)]), !,
	dgraph_add_vertex(Graph0, V, Graph1), 
	add_parents(Parents, V, Graph1, GraphI),
	build_graph(AllVars0, GraphI, Graph).
build_graph(_V.AllVars0, Graph0, Graph) :-
	build_graph(AllVars0, Graph0, Graph).

add_parents([], _V, Graph, Graph).
add_parents(V0.Parents, V, Graph0, GraphF) :-
	dgraph_add_edge(Graph0, V0, V, GraphI), 
	add_parents(Parents, V, GraphI, GraphF).

get_vars_info([], Vs, Vs, Ps, Ps, _, _) --> [].
get_vars_info([V|MoreVs], Vs, VsF, Ps, PsF, Lvs, Outs) -->
	{ clpbn:get_atts(V, [dist(DistId, Parents)]) }, !,
%{writeln(v:DistId:Parents)},
	[DIST],
	{  get_var_info(V, DistId, Parents, Vs, Vs2, Ps, Ps1, Lvs, Outs, DIST) },
	get_vars_info(MoreVs, Vs2, VsF, Ps1, PsF, Lvs, Outs).
get_vars_info([_|MoreVs], Vs0, VsF, Ps0, PsF, VarsInfo, Lvs, Outs) :-
	get_vars_info(MoreVs, Vs0, VsF, Ps0, PsF, VarsInfo, Lvs, Outs).

%
% let's have some fun with avg
%
get_var_info(V, avg(Domain), Parents, Vs, Vs2, Ps, Ps, Lvs, Outs, DIST) :- !,
	length(Domain, DSize),
%	run_though_avg(V, DSize, Domain, Parents, Vs, Vs2, Lvs, Outs, DIST).
%	top_down_with_tabling(V, DSize, Domain, Parents, Vs, Vs2, Lvs, Outs, DIST).
	bup_avg(V, DSize, Domain, Parents, Vs, Vs2, Lvs, Outs, DIST).
% standard random variable
get_var_info(V, DistId, Parents0, Vs, Vs2, Ps, Ps1, Lvs, Outs, DIST) :-
% clpbn:get_atts(V, [key(K)]), writeln(V:K:DistId:Parents),
	reorder_vars(Parents0, Parents, Map),
	check_p(DistId, Map, Parms, _ParmVars, Ps, Ps1),
	unbound_parms(Parms, ParmVars),
	check_v(V, DistId, DIST, Vs, Vs1),
	DIST = info(V, Tree, Ev, Values, Formula, ParmVars, Parms),
	% get a list of form [[P00,P01], [P10,P11], [P20,P21]]
	get_parents(Parents, PVars, Vs1, Vs2),
	cross_product(Values, Ev, PVars, ParmVars, Formula0),
%	(numbervars(Formula0,0,_),writeln(formula0:Ev:Formula0), fail ; true),
	get_evidence(V, Tree, Ev, Formula0, Formula, Lvs, Outs).
%,	(numbervars(Formula,0,_),writeln(formula:Formula), fail ; true)

%
% reorder all variables and make sure we get a
% map of how the transfer was done.
%
% position zero is output
%
reorder_vars(Vs, OVs, Map) :-
	add_pos(Vs, 1, PVs),
	keysort(PVs, SVs),
	remove_key(SVs, OVs, Map).

add_pos([], _, []).
add_pos([V|Vs], I0, [K-(I0,V)|PVs]) :-
	get_atts(V,[order(K)]),
	I is I0+1,
	add_pos(Vs, I, PVs).

remove_key([], [], []).
remove_key([_-(I,V)|SVs], [V|OVs], [I|Map]) :-
	remove_key(SVs, OVs, Map).

%%%%%%%%%%%%%%%%%%%%%%%%%
%
% use top-down to generate average
%
run_though_avg(V, 3, Domain, Parents0, Vs, Vs2, Lvs, Outs, DIST) :-
	reorder_vars(Parents0, Parents, _Map),
	check_v(V, avg(Domain,Parents0), DIST, Vs, Vs1),
	DIST = info(V, Tree, Ev, [V0,V1,V2], Formula, [], []),
	get_parents(Parents, PVars, Vs1, Vs2),
	length(Parents, N),
	generate_3tree(F00, PVars, 0, 0, 0, N, N0, N1, N2, R, (N1+2*N2 =< N/2), (N1+2*(N2+R) =< N/2)),
	simplify_exp(F00, F0),
%	generate_3tree(F1, PVars, 0, 0, 0, N, N0, N1, N2, R, ((N1+2*(N2+R) > N/2, N1+2*N2 < (3*N)/2))),
	generate_3tree(F20, PVars, 0, 0, 0, N, N0, N1, N2, R, (N1+2*(N2+R) >= (3*N)/2), N1+2*N2 >= (3*N)/2),
%	simplify_exp(F20, F2),
	F20=F2,
	Formula0 = [V0=F0*Ev0,V2=F2*Ev2,V1=not(F0+F2)*Ev1],
	Ev = [Ev0,Ev1,Ev2],
	get_evidence(V, Tree, Ev, Formula0, Formula, Lvs, Outs).

generate_3tree(OUT, _, I00, I10, I20, IR0, N0, N1, N2, R, _Exp, ExpF) :-
	IR is IR0-1,
	satisf(I00, I10, I20, IR, N0, N1, N2, R, ExpF),
	!,
	OUT = 1.
generate_3tree(OUT, [[P0,P1,P2]], I00, I10, I20, IR0, N0, N1, N2, R, Exp, _ExpF) :-
	IR is IR0-1,
	( satisf(I00+1, I10, I20, IR, N0, N1, N2, R, Exp) ->
	  L0 = [P0|L1]
	  ;
	  L0 = L1
	),
	( satisf(I00, I10+1, I20, IR, N0, N1, N2, R, Exp) ->
	  L1 = [P1|L2]
	  ;
	  L1 = L2
	),
	( satisf(I00, I10, I20+1, IR, N0, N1, N2, R, Exp) ->
	  L2 = [P2]
	  ;
	  L2 = []
	),
	to_disj(L0, OUT).
generate_3tree(OUT, [[P0,P1,P2]|Ps], I00, I10, I20, IR0, N0, N1, N2, R, Exp, ExpF) :-
	IR is IR0-1,
	( satisf(I00+1, I10, I20, IR, N0, N1, N2, R, Exp) ->
	  I0 is I00+1, generate_3tree(O0, Ps, I0, I10, I20, IR, N0, N1, N2, R, Exp, ExpF)
	  ->
	  L0 = [P0*O0|L1]
	  ;
	  L0 = L1
	),
	( satisf(I00, I10+1, I20, IR0, N0, N1, N2, R, Exp) ->
	  I1 is I10+1, generate_3tree(O1, Ps, I00, I1, I20, IR, N0, N1, N2, R, Exp, ExpF)
	  ->
	  L1 = [P1*O1|L2]
	  ;
	  L1 = L2
	),
	( satisf(I00, I10, I20+1, IR0, N0, N1, N2, R, Exp) ->
	  I2 is I20+1, generate_3tree(O2, Ps, I00, I10, I2, IR, N0, N1, N2, R, Exp, ExpF)
	  ->	  
	  L2 = [P2*O2]
	  ;
	  L2 = []
	),
	to_disj(L0, OUT).


satisf(I0, I1, I2, IR, N0, N1, N2, R, Exp) :-
	\+ \+  ( I0 = N0, I1=N1, I2=N2, IR=R, call(Exp) ).

not_satisf(I0, I1, I2, IR, N0, N1, N2, R, Exp) :-
	\+  ( I0 = N0, I1=N1, I2=N2, IR=R, call(Exp) ).

%%%%%%%%%%%%%%%%%%%%%%%%%
%
% use top-down to generate average
%
top_down_with_tabling(V, Size, Domain, Parents0, Vs, Vs2, Lvs, Outs, DIST) :-
	reorder_vars(Parents0, Parents, _Map),
	check_v(V, avg(Domain,Parents), DIST, Vs, Vs1),
	DIST = info(V, Tree, Ev, OVs, Formula, [], []),
	get_parents(Parents, PVars, Vs1, Vs2),
	length(Parents, N),
	Max is (Size-1)*N, % This should be true
	avg_borders(0, Size, Max, Borders),
	b_hash_new(H0),
	avg_trees(0, Max, PVars, Size, F1, 0, Borders, OVs, Ev, H0, H),
	generate_avg_code(H, Formula, F),
%	Formula0 = [V0=F0*Ev0,V2=F2*Ev2,V1=not(F0+F2)*Ev1],
%	Ev = [Ev0,Ev1,Ev2],
	get_evidence(V, Tree, Ev, F1, F, Lvs, Outs).

avg_trees(Size, _, _, Size, F0, _, F0, [], [], H, H) :- !.
avg_trees(I0, Max, PVars, Size, [V=O*E|F0], Im, [IM|Borders], [V|OVs], [E|Ev], H0, H) :-
	I is I0+1,
	avg_tree(PVars, 0, Max, Im, IM, Size, O, H0, HI),
	Im1 is IM+1,
	avg_trees(I, Max, PVars, Size, F0, Im1, Borders, OVs, Ev, HI, H).

avg_tree( _PVars, P, _, Im, IM, _Size, O, H0, H0) :-
	b_hash_lookup(k(P,Im,IM), O=_Exp, H0), !.
avg_tree([], _P, _Max, _Im, _IM, _Size, 1, H, H).
avg_tree([Vals|PVars], P, Max, Im, IM, Size, O, H0, HF) :-
	b_hash_insert(H0, k(P,Im,IM), O=Simp*1, HI),
	MaxI is Max-(Size-1),
	avg_exp(Vals, PVars, 0, P, MaxI, Size, Im, IM, HI, HF, Exp),
	simplify_exp(Exp, Simp).
	
avg_exp([], _, _, _P, _Max, _Size, _Im, _IM, H, H, 0).
avg_exp([Val|Vals], PVars, I0, P0, Max, Size, Im, IM, HI, HF, O) :-
	(Vals = [] -> O=O1 ; O = Val*O1+not(Val)*O2 ),
	Im1 is max(0, Im-I0),
	IM1 is IM-I0,
	( IM1 < 0 -> O1 = 0, H2 = HI;      /* we have exceed maximum */
	  Im1 > Max -> O1 = 0, H2 = HI;  /* we cannot make to minimum */
	  Im1 = 0, IM1 > Max -> O1 = 1, H2 = HI;  /* we cannot exceed maximum */
	  P is P0+1,
	  avg_tree(PVars, P, Max, Im1, IM1, Size, O1, HI, H2)
        ),
	I is I0+1,
	avg_exp(Vals, PVars, I, P0, Max, Size, Im, IM, H2, HF, O2).

generate_avg_code(H, Formula, Formula0) :-
	b_hash_to_list(H,L),
	sort(L, S),
	strip_and_add(S, Formula0, Formula).

strip_and_add([], F, F).
strip_and_add([_-Exp|S], F0, F) :-
	strip_and_add(S, [Exp|F0], F).

%%%%%%%%%%%%%%%%%%%%%%%%%
%
% use bottom-up dynamic programming to generate average
%
bup_avg(V, Size, Domain, Parents0, Vs, Vs2, Lvs, Outs, DIST) :-
	reorder_vars(Parents0, Parents, _),
	check_v(V, avg(Domain,Parents), DIST, Vs, Vs1),
	DIST = info(V, Tree, Ev, OVs, Formula, [], []),
	get_parents(Parents, PVars, Vs1, Vs2),
	length(Parents, N),
	Max is (Size-1)*N, % This should be true
	ArraySize is Max+1,
	functor(Protected, protected, ArraySize),
	avg_domains(0, Size, 0, Max, LDomains),
	Domains =.. [d|LDomains],
	Reach is (Size-1),
	generate_sums(PVars, Size, Max, Reach, Protected, Domains, ArraySize, Sums, F0),
%	bin_sums(PVars, Sums, F00),
%	reverse(F00,F0),
	% easier to do recursion on lists
	Sums =.. [_|LSums],
	generate_avg(0, Size, 0, Max, LSums, OVs, Ev, F1, []),
	reverse(F0, RF0),
	get_evidence(V, Tree, Ev, F1, F2, Lvs, Outs),
	append(RF0, F2, Formula).

%
% use binary approach, like what is standard
%
bin_sums(Vs, Sums, F) :-
	 vs_to_sums(Vs, Sums0),
	 bin_sums(Sums0, Sums, F, []).
	
vs_to_sums([], []).
vs_to_sums([V|Vs], [Sum|Sums0]) :-
	  Sum =.. [sum|V],
	  vs_to_sums(Vs, Sums0).

bin_sums([Sum], Sum) --> !.
bin_sums(LSums, Sum) --> 
	{ halve(LSums, Sums1, Sums2) },
	bin_sums(Sums1, Sum1),
	bin_sums(Sums2, Sum2),
	sum(Sum1, Sum2, Sum).

halve(LSums, Sums1, Sums2) :-
	length(LSums, L),
	Take is L div 2,
	head(Take, LSums, Sums1, Sums2).

head(0, L, [], L) :- !.
head(Take, [H|L], [H|Sums1], Sum2) :-
	Take1 is Take-1,
	head(Take1, L, Sums1, Sum2).

sum(Sum1, Sum2, Sum) -->
	  { functor(Sum1, _, M1),
	    functor(Sum2, _, M2),
	    Max is M1+M2-2,
	    Max1 is Max+1,
	    Max0 is M2-1,
	    functor(Sum, sum, Max1),
	    Sum1 =.. [_|PVals] },
	  expand_sums(PVals, 0, Max0, Max1, M2, Sum2, Sum).

%
% bottom up step by step
%
%
generate_sums([PVals], Size, Max, _, _Protected, _Domains, _, Sum, []) :- !,
	Max is Size-1,
	Sum =.. [sum|PVals].
generate_sums([PVals|Parents], Size, Max, Reach, Protected, Domains, ASize, NewSums, F) :-
	NewReach is Reach+(Size-1),
	generate_sums(Parents, Size, Max0, NewReach, Protected, Domains, ASize, Sums, F0),
	Max is Max0+(Size-1),
	Max1 is Max+1,
	functor(NewSums, sum, Max1),
	protect_avg(0, Max0, Protected, Domains, ASize, Reach),
	expand_sums(PVals, 0, Max0, Max1, Size, Sums, Protected, NewSums, F, F0).

protect_avg(Max0,Max0,_Protected, _Domains, _ASize, _Reach) :- !.
protect_avg(I0, Max0, Protected, Domains, ASize, Reach) :-
	I is I0+1,
	Top is I+Reach,
	( Top > ASize ;
	    arg(I, Domains, CD),
	    arg(Top, Domains, CD)
	), !,
	arg(I, Protected, yes),
	protect_avg(I, Max0, Protected, Domains, ASize, Reach).
protect_avg(I0, Max0, Protected, Domains, ASize, Reach) :-
	I is I0+1,
	protect_avg(I, Max0, Protected, Domains, ASize, Reach).


%
% outer loop: generate array of sums at level j= Sum[j0...jMax]
%
expand_sums(_Parents, Max, _, Max, _Size, _Sums, _P, _NewSums, F0, F0) :- !.
expand_sums(Parents, I0, Max0, Max, Size, Sums, Prot, NewSums, [O=SUM*1|F], F0) :-
	I is I0+1,
	arg(I, Prot, P),
	var(P), !,
	arg(I, NewSums, O),
	sum_all(Parents, 0, I0, Max0, Sums, List),
	to_disj(List, SUM),
        expand_sums(Parents, I, Max0, Max, Size, Sums, Prot, NewSums, F, F0).
expand_sums(Parents, I0, Max0, Max, Size, Sums, Prot, NewSums, F, F0) :-
	I is I0+1,
	arg(I, Sums, O),
	arg(I, NewSums, O),
        expand_sums(Parents, I, Max0, Max, Size, Sums, Prot, NewSums, F, F0).

%
%inner loop: find all parents that contribute to A_ji,
% that is generate Pk*Sum_(j-1)l and k+l st k+l = i
%
sum_all([], _, _, _, _, []).
sum_all([V|Vs], Pos, I, Max0, Sums, [O|List]) :-
	J is I-Pos,
	J >= 0,
	J =< Max0, !,
	J1 is J+1,
	arg(J1, Sums, S0),
	( J < I -> O = V*S0 ; O = S0*V ),
	Pos1 is Pos+1,
	sum_all(Vs, Pos1, I, Max0, Sums, List).
sum_all([_V|Vs], Pos, I, Max0, Sums, List) :-
	Pos1 is Pos+1,
	sum_all(Vs, Pos1, I, Max0, Sums, List).

gen_arg(J, Sums, Max, S0) :-
	gen_arg(0, Max, J, Sums, S0).
	
gen_arg(Max, Max, J, Sums, S0) :- !,
	     I is Max+1,
	     arg(I, Sums, A),
	( Max = J -> S0 = A ; S0 = not(A)).
gen_arg(I0, Max, J, Sums, S) :-
	     I is I0+1,
	     arg(I, Sums, A),
	( I0 = J -> S = A*S0 ; S = not(A)*S0),
	gen_arg(I, Max, J, Sums, S0).


avg_borders(Size, Size, _Max, []) :- !.
avg_borders(I0, Size, Max, [J|Vals]) :-
	I is I0+1,
	Border is (I*Max)/Size,
	J is integer(round(Border)),
	avg_borders(I, Size, Max, Vals).

avg_domains(Size, Size, _J, _Max, []).
avg_domains(I0, Size, J0, Max, Vals) :-
	I is I0+1,
	Border is (I*Max)/Size,
	fetch_domain_for_avg(J0, Border, J, I0, Vals, ValsI),
	avg_domains(I, Size, J, Max, ValsI).

fetch_domain_for_avg(J, Border, J, _, Vals, Vals) :-
	J > Border, !.
fetch_domain_for_avg(J0, Border, J, I0, [I0|LVals], RLVals) :-
	J1 is J0+1,
	fetch_domain_for_avg(J1, Border, J, I0, LVals, RLVals).

generate_avg(Size, Size, _J, _Max, [], [], [], F, F).
generate_avg(I0, Size, J0, Max, LSums, [O|OVs], [Ev|Evs], [O=Disj*Ev|F], F0) :-
	I is I0+1,
	Border is (I*Max)/Size,
	fetch_for_avg(J0, Border, J, LSums, MySums, RSums),
	to_disj(MySums, Disj),
	generate_avg(I, Size, J, Max, RSums, OVs, Evs, F, F0).

fetch_for_avg(J, Border, J, RSums, [], RSums) :-
	J > Border, !.
fetch_for_avg(J0, Border, J, [S|LSums], [S|MySums], RSums) :-
	J1 is J0+1,
	fetch_for_avg(J1, Border, J, LSums, MySums, RSums).


to_disj([], 0).
to_disj([V], V).
to_disj([V,V1|Vs], Out) :-
	to_disj2([V1|Vs], V, Out).

to_disj2([V], V0, V0+V).
to_disj2([V,V1|Vs], V0, Out) :-
	to_disj2([V1|Vs], V0+V, Out).


%
% look for parameters in the rb-tree, or add a new.
% distid is the key
%
check_p(DistId, Map, Parms, ParmVars, Ps, Ps) :-
	rb_lookup(DistId-Map, theta(Parms, ParmVars), Ps), !.
check_p(DistId, Map, Parms, ParmVars, Ps, PsF) :-
	get_dist_params(DistId, Parms0),
	get_dist_all_sizes(DistId, Sizes),
	swap_parms(Parms0, Sizes, [0|Map], Parms1),
	length(Parms1, L0),
	get_dist_domain_size(DistId, Size),
	L1 is L0 div Size,
	L is L0-L1,
	initial_maxes(L1, Multipliers),
	copy(L, Multipliers, NextMults, NextMults, Parms1, Parms, ParmVars),
%writeln(t:Size:Parms0:Parms:ParmVars),
	rb_insert(Ps, DistId-Map, theta(Parms, ParmVars), PsF).

swap_parms(Parms0, Sizes, Map, Parms1) :-
	matrix_new(floats, Sizes, Parms0, T0),
	matrix_shuffle(T0,Map,TF),
	matrix_to_list(TF, Parms1).

%
% we are using switches by two
%
initial_maxes(0, []) :- !.
initial_maxes(Size, [1.0|Multipliers]) :- !,
	Size1 is Size-1,
	initial_maxes(Size1, Multipliers).

copy(0, [], [], _, _Parms0, [], []) :- !.
copy(N, [], [], Ms, Parms0, Parms, ParmVars) :-!,
	copy(N, Ms, NewMs, NewMs, Parms0, Parms, ParmVars).
copy(N, D.Ds, ND.NDs, New, El.Parms0, NEl.Parms, V.ParmVars) :-
	N1 is N-1,
	(El == 0.0 -> 
	    NEl = 0,
	    V = NEl,
	    ND = D
	;El == 1.0 -> 
	    NEl = 1,
	    V = NEl,
	    ND = 0.0
	;El == 0 -> 
	    NEl = 0,
	    V = NEl,
	    ND = D
	;El =:= 1 -> 
	    NEl = 1,
	    V = NEl,
	    ND = 0.0,
	    V = NEl
	;
	    NEl is El/D,
	    ND is D-El,
	    V = NEl
	),
	copy(N1, Ds, NDs, New, Parms0, Parms, ParmVars).

unbound_parms([], []).
unbound_parms(_.Parms, _.ParmVars) :-
	  unbound_parms(Parms, ParmVars).

check_v(V, _, INFO, Vs, Vs) :-
	rb_lookup(V, INFO, Vs), !.
check_v(V, DistId, INFO, Vs0, Vs) :-
	get_dist_domain_size(DistId, Size),
	length(Values, Size),
	length(Ev, Size),
	INFO = info(V, _Tree, Ev, Values, _Formula, _, _),
	rb_insert(Vs0, V, INFO, Vs).

get_parents([], [], Vs, Vs).
get_parents(V.Parents, Values.PVars, Vs0, Vs) :-
	clpbn:get_atts(V, [dist(DistId, _)]),
	check_v(V, DistId, INFO, Vs0, Vs1),
	INFO = info(V, _Parent, _Ev, Values, _, _, _),
	get_parents(Parents, PVars, Vs1, Vs).

%
% construct the formula, this is the key...
%
cross_product(Values, Ev, PVars, ParmVars, Formulas) :-
	arrangements(PVars, Arranges),
	apply_parents_first(Values, Ev, ParmCombos, ParmCombos, Arranges, Formulas, ParmVars).

%
% if we have the parent variables with two values, we get
% [[XP,YP],[XP,YN],[XN,YP],[XN,YN]]
%
arrangements([], [[]]).
arrangements([L1|Ls],O) :-
	arrangements(Ls, LN),
	expand(L1, LN, O, []).

expand([], _LN) --> [].
expand([H|L1], LN) -->
	concatenate_all(H, LN),
	expand(L1, LN).

concatenate_all(_H, []) --> [].
concatenate_all(H, L.LN) -->
	[[H|L]],
	concatenate_all(H, LN).

%
% core of algorithm
%
% Values -> Output Vars for BDD
% Es -> Evidence variables
% Previous -> top of difference list with parameters used so far
% P0 -> end of difference list with parameters used so far
% Pvars -> Parents
% Eqs -> Output Equations
% Pars -> Output Theta Parameters
%
apply_parents_first([Value], [E], Previous, [], PVars, [Value=Disj*E], Parameters) :- !,
	apply_last_parent(PVars, Previous, Disj),
	flatten(Previous, Parameters).
apply_parents_first([Value|Values], [E|Ev], Previous, P0, PVars, (Value=Disj*E).Formulas, Parameters) :-
	P0 = [TheseParents|End],
	apply_first_parent(PVars, Disj, TheseParents),
	apply_parents_second(Values, Ev, Previous, End, PVars, Formulas, Parameters).

apply_parents_second([Value], [E], Previous, [], PVars, [Value=Disj*E], Parameters) :- !,
	apply_last_parent(PVars, Previous, Disj),
	flatten(Previous, Parameters).
apply_parents_second([Value|Values], [E|Ev], Previous, P0, PVars, (Value=Disj*E).Formulas, Parameters) :-
	apply_middle_parent(PVars, Previous, Disj, TheseParents),
	% this must be done after applying middle parents because of the var
	% test.
	P0 = [TheseParents|End],
	apply_parents_second(Values, Ev, Previous, End, PVars, Formulas, Parameters).

apply_first_parent([Parents], Conj, [Theta]) :- !,
	parents_to_conj(Parents,Theta,Conj).
apply_first_parent(Parents.PVars, Conj+Disj, Theta.TheseParents) :-
	parents_to_conj(Parents,Theta,Conj),
	apply_first_parent(PVars, Disj, TheseParents).

apply_middle_parent([Parents], Other, Conj, [ThetaPar]) :- !,
	skim_for_theta(Other, Theta, _, ThetaPar),
	parents_to_conj(Parents,Theta,Conj).
apply_middle_parent(Parents.PVars, Other, Conj+Disj, ThetaPar.TheseParents) :-
	skim_for_theta(Other, Theta, Remaining, ThetaPar),
	parents_to_conj(Parents,(Theta),Conj),
	apply_middle_parent(PVars, Remaining, Disj, TheseParents).

apply_last_parent([Parents], Other, Conj) :- !,
	parents_to_conj(Parents,(Theta),Conj),
	skim_for_theta(Other, Theta, _, _).
apply_last_parent(Parents.PVars, Other, Conj+Disj) :-
	parents_to_conj(Parents,(Theta),Conj),
	skim_for_theta(Other, Theta, Remaining, _),
	apply_last_parent(PVars, Remaining, Disj).

%
%
% simplify stuff, removing process that is cancelled by 0s
%
parents_to_conj([], Theta, Theta) :- !.
parents_to_conj(Ps, Theta, Theta*Conj) :-
	parents_to_conj2(Ps, Conj).

parents_to_conj2([P],P) :- !.
parents_to_conj2(P.Ps,P*Conj) :-
	parents_to_conj2(Ps,Conj).

%
% first case we haven't reached the end of the list so we need
% to create a new parameter variable
%
skim_for_theta([[P|Other]|V], not(P)*New, [Other|_], New) :- var(V), !.
%
% last theta, it is just negation of the other ones
%
skim_for_theta([[P|Other]], not(P), [Other], _) :- !.
%
% recursive case, build-up
%
skim_for_theta([[P|Other]|More], not(P)*Ps, [Other|Left], New ) :-
	skim_for_theta(More, Ps, Left, New ).

get_evidence(V, Tree, Ev, F0, F, Leaves, Finals) :-
	clpbn:get_atts(V, [evidence(Pos)]), !,
	zero_pos(0, Pos, Ev),
	insert_output(Leaves, V, Finals, Tree, Outs, SendOut),
	get_outs(F0, F, SendOut, Outs).
% hidden deterministic node, can be removed.
get_evidence(V, _Tree, Ev, F0, [], _Leaves, _Finals) :-
	clpbn:get_atts(V, [key(K)]),
	functor(K, Name, 2),
	( Name = 'AVG' ; Name = 'MAX' ; Name = 'MIN' ),
	!,
	one_list(Ev),
	eval_outs(F0).	
%% no evidence !!!
get_evidence(V, Tree, _Values, F0, F1, Leaves, Finals) :-
	insert_output(Leaves, V, Finals, Tree, Outs, SendOut),
	get_outs(F0, F1, SendOut, Outs).

zero_pos(_, _Pos, []).
zero_pos(Pos, Pos, 1.Values) :- !, 
	I is Pos+1,
	zero_pos(I, Pos, Values).
zero_pos(I0, Pos, 0.Values) :- 
	I is I0+1,
	zero_pos(I, Pos, Values).

one_list([]).
one_list(1.Ev) :-
	one_list(Ev).

%
% insert a node with the disj of all alternatives, this is only done if node ends up to be in the output 
%
insert_output([], _V, [], _Out, _Outs, []).
insert_output(V._Leaves, V0, [Top|_], Top, Outs, [Top = Outs]) :- V == V0, !.
insert_output(_.Leaves, V, _.Finals, Top, Outs, SendOut) :-
	insert_output(Leaves, V, Finals, Top, Outs, SendOut).


get_outs([V=F], [V=NF|End], End,  V) :- !,
%	writeln(f0:F),
	simplify_exp(F,NF).
get_outs((V=F).Outs, (V=NF).NOuts, End, (F0 + V)) :-
%	writeln(f0:F),
	simplify_exp(F,NF),
	get_outs(Outs, NOuts, End, F0).

eval_outs([]).
eval_outs((V=F).Outs) :-
	simplify_exp(F,NF),
	V = NF,
	eval_outs(Outs).

run_bdd_solver([[V]], LPs, bdd(Term, _Leaves, Nodes)) :-
	build_out_node(Nodes, Node),
	findall(Prob, get_prob(Term, Node, V, Prob),TermProbs),
	sumlist(TermProbs, Sum),
	normalise(TermProbs, Sum, LPs).

build_out_node([_Top], []).
build_out_node([T,T1|Tops], [Top = T*Top]) :-
	build_out_node2(T1.Tops, Top).

build_out_node2([Top], Top).
build_out_node2([T,T1|Tops], T*Top) :-
	build_out_node2(T1.Tops, Top).


get_prob(Term, _Node, V, SP) :-
	bdds(ddnnf), !,
	all_cnfs(Term, CNF, IVs, Indics, V, AllParms, AllParmValues),
	build_cnf(CNF, IVs, Indics, AllParms, AllParmValues, SP).
get_prob(Term, Node, V, SP) :-
	bdds(bdd), !,
	bind_all(Term, Node, Bindings, V, AllParms, AllParmValues),
%	reverse(AllParms, RAllParms),
	term_variables(AllParms, NVs),
	build_bdd(Bindings, NVs, AllParms, AllParmValues, Bdd),
	bdd_to_probability_sum_product(Bdd, SP),
	bdd_close(Bdd).

build_bdd(Bindings, NVs, VTheta, Theta, Bdd) :-
	bdd_from_list(Bindings, NVs, Bdd),
%	bdd_size(Bdd, Len),
%	number_codes(Len,Codes),
%	atom_codes(Name,Codes),
%	bdd_print(Bdd, Name),
%	writeln(length=Len),
	VTheta = Theta.

bind_all([], End, End, _V, [], []).
bind_all([info(V, _Tree, Ev, _Values, Formula, ParmVars, Parms)|Term], End, BindsF, V0, ParmVars.AllParms, Parms.AllTheta) :-
	V0 == V, !,
	set_to_one_zeros(Ev),
	bind_formula(Formula, BindsF, BindsI),
	bind_all(Term, End, BindsI, V0, AllParms, AllTheta).
bind_all([info(_V, _Tree, Ev, _Values, Formula, ParmVars, Parms)|Term], End, BindsF, V0, ParmVars.AllParms, Parms.AllTheta) :-
	set_to_ones(Ev),!,
	bind_formula(Formula, BindsF, BindsI),
	bind_all(Term, End, BindsI, V0, AllParms, AllTheta).
% evidence: no need to add any stuff.
bind_all([info(_V, _Tree, _Ev, _Values, Formula, ParmVars, Parms)|Term], End, BindsF, V0, ParmVars.AllParms, Parms.AllTheta) :-
	bind_formula(Formula, BindsF, BindsI),
	bind_all(Term, End, BindsI, V0, AllParms, AllTheta).

bind_formula([], L, L).
bind_formula(B.Formula, B.BsF, Bs0) :-
	bind_formula(Formula, BsF, Bs0).

set_to_one_zeros([1|Values]) :-
	set_to_zeros(Values).
set_to_one_zeros([0|Values]) :-
	set_to_one_zeros(Values).

set_to_zeros([]).
set_to_zeros(0.Values) :-
	set_to_zeros(Values).

set_to_ones([]).
set_to_ones(1.Values) :-
	set_to_ones(Values).

normalise([], _Sum, []).
normalise(P.TermProbs, Sum, NP.LPs) :-
	NP is P/Sum,
	normalise(TermProbs, Sum, LPs).

finalize_bdd_solver(_).

all_cnfs([], [], [], [], _V, [], []).
all_cnfs([info(V, Tree, Ev, Values, Formula, ParmVars, Parms)|Term], BindsF, IVars, Indics, V0, AllParmsF, AllThetaF) :-
%writeln(f:Formula),
	V0 == V, !,
	set_to_one_zeros(Ev),
	all_indicators(Values, BindsF, Binds0),
	indicators(Values, [], Ev, IVars, IVarsI, Indics, IndicsI, Binds0, Binds1),
	parms( ParmVars, Parms, AllParmsF, AllThetaF, AllParms, AllTheta),
	parameters(Formula, Tree, Binds1, BindsI),
	all_cnfs(Term, BindsI, IVarsI, IndicsI, V0, AllParms, AllTheta).
all_cnfs([info(_V, Tree, Ev, Values, Formula, ParmVars, Parms)|Term], BindsF, IVars, Indics, V0, AllParmsF, AllThetaF) :-
	set_to_ones(Ev),!,
	all_indicators(Values, BindsF, Binds0),
	indicators(Values, [], Ev, IVars, IVarsI, Indics, IndicsI, Binds0, Binds1),
	parms( ParmVars, Parms, AllParmsF, AllThetaF, AllParms, AllTheta),
	parameters(Formula, Tree, Binds1, BindsI),
	all_cnfs(Term, BindsI, IVarsI, IndicsI, V0, AllParms, AllTheta).
% evidence: no need to add any stuff.
all_cnfs([info(_V, Tree, Ev, Values, Formula, ParmVars, Parms)|Term], BindsF, IVars, Indics, V0, AllParmsF, AllThetaF) :-
	all_indicators(Values, BindsF, Binds0),
	indicators(Values, [], Ev, IVars, IVarsI, Indics, IndicsI, Binds0, Binds1),
	parms( ParmVars, Parms, AllParmsF, AllThetaF, AllParms, AllTheta),
	parameters(Formula, Tree, Binds1, BindsI),
	all_cnfs(Term, BindsI, IVarsI, IndicsI, V0, AllParms, AllTheta).

all_indicators(Values) -->
	{ values_to_disj(Values, Disj) },
	[Disj].

values_to_disj([V], V) :- !.
values_to_disj([V|Values], V+Disj) :-
	values_to_disj(Values, Disj).

indicators([V|Vars], SeenVs, [E|Ev], [V|IsF], IsI, [E|Inds], Inds0) -->
	generate_exclusions(SeenVs, V),
	indicators(Vars, [V|SeenVs], Ev, IsF, IsI, Inds, Inds0).
indicators([], _SeenVs, [], IsF, IsF, Inds, Inds) --> [].

parms([], [], AllParms, AllTheta, AllParms, AllTheta).
parms([V|ParmVars], [P|Parms], [V|AllParmsF], [P|AllThetaF], AllParms, AllTheta) :-
	parms( ParmVars, Parms, AllParmsF, AllThetaF, AllParms, AllTheta).

parameters([], _) --> [].
% ignore disj, only useful to BDDs
parameters([(T=_)|Formula], Tree) -->
	{ Tree == T }, !,
	parameters(Formula, Tree).
parameters([(V0=Disj*_I0)|Formula], Tree) -->
	conj(Disj, V0),
	parameters(Formula, Tree).

% transform V0<- A*B+C*(D+not(E))
%        [V0+not(A)+not(B),V0+not(C)+not(D),V0+not(C)+E]
conj(Disj, V0) -->
	{ conj2(Disj, [[V0]], LVs) },
	to_disjs(LVs).

conj2(A, L0, LF) :- var(A), !,
	add(not(A), L0, LF).
conj2((A*B), L0, LF) :-
	conj2(A, L0, LI),
	conj2(B, LI, LF).
conj2((A+B), L0, LF) :-
	conj2(A, L0, L1),
	conj2(B, L0, L2),
	append(L1, L2, LF).
conj2(not(A), L0, LF) :-
	add(A, L0, LF).

add(_, [], []).
add(Head, [H|L], [[Head|H]|NL]) :-
	add(Head, L, NL).

to_disjs([]) --> [].
to_disjs([[H|L]|LVs]) -->
	mkdisj(L, H),
	to_disjs(LVs).

mkdisj([], Disj) --> [Disj].
mkdisj([H|L], Disj) -->
	mkdisj(L, (H+Disj)).

%
% add formula for V \== V0 -> V or V0 and not(V) or not(V0)
%
generate_exclusions([], _V) --> [].
generate_exclusions([V0|SeenVs], V) -->
	[(not(V0)+not(V))],
	generate_exclusions(SeenVs, V).

build_cnf(CNF, IVs, Indics, AllParms, AllParmValues, Val) :-
%(numbervars(CNF,1,_), writeln(cnf_to_ddnnf(CNF, Vars, IVs, [], F)), fail ; true ),
	cnf_to_ddnnf(CNF, AllParms, F), 
	AllParms = AllParmValues,
	IVs = Indics,
	term_variables(CNF, Extra),
	set_to_ones(Extra),
	ddnnf_is(F, Val).


