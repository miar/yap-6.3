:- yap_flag(tabling_mode,[local,load_answers,local_trie]).

%:- ['data_size_1600_max_random_160_k_3200.pl'].

%:- ['misc/data_example.pl'].
:-['knapsack_depth_first_rule.pl'].


run_query(T):- 
  data_call(I, K),
  statistics(walltime,[InitTime,_]),
  forall(between(1,T,_),thread_create(time_query(I, K),_)),
  forall(between(1,T,Tid2), thread_join(Tid2,_)), 
  statistics(walltime,[EndTime,_]),
  Time is EndTime - InitTime,
  writeln('Threads-Time:'-T-Time).
%  knapsack(I, K, V),
%  writeln('Result'-V).
  

%:- run_query(32).
%:- current_output(Stream), show_walltime_by_thread(Stream).
%:-tabling_statistics.
%:-halt.
