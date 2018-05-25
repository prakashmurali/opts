#include "main.h"

using namespace std;

QParams gParams;
QStats gStats;

SMTOutput *create_and_check_instance(Circuit *pCircuit, Machine *pMachine, int mid){
  // Creates and checks and instance where the allowed scheduling time is bounded by mid
  context c;
  //SolverType opt(c, "QF_LIA");
  SolverType opt(c);

  cout << "------------------------------\n";
  SMTSchedule *pSolver = new SMTSchedule(pCircuit, pMachine);
  pSolver->create_optimization_instance(c, opt, TRUE);
  cout << "Checking " << mid << "\n";
  SMTOutput *pOut = pSolver->check_instance(c, opt, mid);
  stats pStats = opt.statistics();
  accumulate_stats(pStats);
  delete pSolver;
  return (pOut);
}

void _test_an_instance(Circuit *pCircuit, Machine *pMachine, int T){
  SMTOutput *pOut = create_and_check_instance(pCircuit, pMachine, T);
  if(pOut->result == IS_SAT){
    cout << "IS SAT\n";
  }else{
    cout << "IS UNSAT\n";
  }
}

SMTOutput* near_optimal_search(Circuit *pCircuit, Machine *pMachine, float epsilon){
  assert(epsilon > 1);
  try{
    int lower = 0;
    int upper = gParams.maxTimeSlot;
    cout << "Solving upper bound\n";
    SMTOutput *pOutUpper = create_and_check_instance(pCircuit, pMachine, upper);

    if(pOutUpper->result == IS_UNSAT){
      cout << "Upper bound UNSAT\n";
      return 0;
    }

    SMTOutput *pSol = pOutUpper;
    float approx_ratio;
    while(lower < upper){
      int mid = lower + (upper-lower)/2;
      cout << "Checking " << mid << "\n";
      SMTOutput *pOut = create_and_check_instance(pCircuit, pMachine, mid);
      if(pOut->result == IS_SAT){
        upper = mid;
        //delete pSol here
        pSol = pOut;
      }else{
        lower = mid;
      }
      approx_ratio = (float)upper/(lower+1);
      if(approx_ratio <= epsilon){
        cout << "Found near optimal solution, lower: " << lower << " upper: " << upper << "\n"; 
        break;
      }else{
        cout << "Found an approx solution, lower: " << lower << " upper: " << upper << "\n"; 
      }
    }
    return pSol;
  }
  catch(const z3::exception &e){
    cerr << e;
  }
}

int solve_mapping(Circuit *pCircuit, Machine *pMachine){
  try{
    context c;
    SolverType opt(c);
    SMTSchedule *pSolver = new SMTSchedule(pCircuit, pMachine);
    pSolver->ts_create_mapping_instance(c, opt);
    delete pSolver;
  }
  catch(const z3::exception &e){
    cerr << e;
  }
  return 0;
}

SMTOutput* _test_lr_instance(Circuit *pCircuit, Machine *pMachine, int T){
  context c;
  SolverType opt(c);
  SMTSchedule *pSolver = new SMTSchedule(pCircuit, pMachine);
  SMTOutput *pOut = pSolver->lr_greedy_solve(c, opt, T);
  delete pSolver;
  return (pOut);
}

typedef SMTOutput* (*solver_func)(Circuit *, Machine *, SMTOutput *, int);
SMTOutput *fire_param_tune(solver_func pFunc, Circuit *pC, Machine *pM, SMTOutput *pGiven, int upper){
  try{
    SMTOutput *pOutUpper = pFunc(pC, pM, pGiven, upper); 
    if(pOutUpper->result == IS_UNSAT){
      cout << "Upper bound UNSAT\n";
      return 0;
    }
    int lower = 0;
    SMTOutput *pSol = pOutUpper;
    float approx_ratio;
    while(lower < upper){
      int mid = lower + (upper-lower)/2;
      cout << "Checking " << mid << "\n";
      SMTOutput *pOut = pFunc(pC, pM, pGiven, mid);
      if(pOut->result == IS_SAT){
        upper = mid;
        delete pSol;
        pSol = pOut;
      }else{
        lower = mid;
      }
      approx_ratio = (float)upper/(lower+1);
      if(approx_ratio <= 1.1){
        cout << "Approx. factor " << approx_ratio << " \nFound near optimal solution, lower: " << lower << " upper: " << upper << "\n"; 
        break;
      }else{
        cout << "Approx. factor " << approx_ratio << " \nCurrent solution, lower: " << lower << " upper: " << upper << "\n"; 
      }
    }
    return pSol;
  }
  catch(const z3::exception &e){
    cerr << e;
    return 0;
  }
}



SMTOutput* test_ms_instance(Circuit *pC, Machine *pM, SMTOutput *pGiven, int T1){
  context c;
  SolverType opt(c);
  SMTSchedule *pSolver = new SMTSchedule(pC, pM);
  pSolver->create_optimization_instance(c, opt, FALSE);
  SMTOutput *pOut = pSolver->check_instance(c, opt, T1);
  delete pSolver;
  return pOut;
}

SMTOutput* test_sr_instance(Circuit *pC, Machine *pM, SMTOutput *pGiven, int T2){
  context sr_ctx;
  SolverType sr_opt(sr_ctx);
  SMTSchedule *pSolver = new SMTSchedule(pC, pM);
  pSolver->create_sr_instance(sr_ctx, sr_opt, pGiven);
  SMTOutput *pOut = pSolver->check_instance(sr_ctx, sr_opt, T2); 
  delete pSolver;
  return pOut;
}

SMTOutput* test_ms_sr(Circuit *pC, Machine *pM, int T1, int T2){
  int stime, etime;
  cout << "\nMS Step: solving with " << T1 << "\n\n";
  stime = MyGetTime();
  
  //SMTOutput *pOut = fire_param_tune(test_ms_instance, pC, pM, 0, T1);
  SMTOutput *pOut = test_ms_instance(pC, pM, 0, T1);

  etime = MyGetTime();
  cout << "MS Solve Time:" << etime-stime << "\n";
  cout << "\nSR Step: solving with " << T2 << "\n\n";
  stime = MyGetTime();

  SMTOutput *pFinalOut = fire_param_tune(test_sr_instance, pC, pM, pOut, T2);
  
  etime = MyGetTime();
  cout << "SR Solve Time:" << etime-stime << "\n";
  delete pOut;
  return pFinalOut;
}

SMTOutput *find_ejf_schedule(Circuit *pC, Machine *pM, SMTOutput *pGiven){
  EJFSchedule *pSched = new EJFSchedule(pC, pM, pGiven);
  pSched->compute_schedule();
}

int main(int argc, char **argv){
  gParams.time_CNOT = 8;
  gParams.time_X = 2;
  gParams.time_Y = 2;
  gParams.time_H = 1;
  gParams.time_MeasZ = 5;
  cout << argc << endl;
  if(argc != 7) assert(0);

  gParams.couplingThresh = 0;
  gParams.transitiveClFlag = 0;

  Circuit *pCircuit = new Circuit; 
  string circuitPrefix = (string)argv[1];
  gParams.machineRows = atoi(argv[2]);
  gParams.machineCols = atoi(argv[3]);
  gParams.dataAwareFlag = atoi(argv[4]);
  gParams.routingPolicy = atoi(argv[5]);
  int T2 = atoi(argv[6]);
  gParams.maxTimeSlot = T2;

  pCircuit->input_from_file(circuitPrefix + ".in");
  pCircuit->input_cnot_overlap_info(circuitPrefix + ".rr");
  pCircuit->input_gate_descendants(circuitPrefix + ".des");
  pCircuit->input_program_graph(circuitPrefix + ".pg");
  pCircuit->input_greedy_mapping(circuitPrefix + ".gmap");
  
  assert(gParams.routingPolicy == 1 || gParams.routingPolicy == 2);

  cout << "Experiment configuration\n-----\n";
  cout << "Program:" << circuitPrefix << "\n";
  cout << "Machine:" << gParams.machineRows << "," << gParams.machineCols << "\n";
  cout << "Data aware:" << gParams.dataAwareFlag << "\n";
  cout << "Routing policy:" << gParams.routingPolicy << "\n";
  cout << "SR T:" << T2 << "\n";
  cout << "Global T:" << T2 << "\n";
  cout << "-----\n";

  Machine *pMachine = new Machine;
  pMachine->setup_grid_transmons(gParams.machineRows, gParams.machineCols);
  pMachine->setup_gate_times();
  if(gParams.dataAwareFlag == 1){
    /* NOTE: This is only supported for ibmqx5 */
    pMachine->input_coherence_data("data/coh.txt");
    pMachine->input_rr_cnot_data("data/cnot_rr_time.txt");
    pMachine->input_one_bend_cnot_data("data/cnot_1bend_time.txt");
  }
  
  int T = gParams.maxTimeSlot;

  int stime, etime;
  init_stats();  //initialize statistics counters, accumulated across the binary search runs
  
  //Main compilation routine
  stime = MyGetTime();
  SMTOutput *pOut = near_optimal_search(pCircuit, pMachine, 1.1);
  etime = MyGetTime();

  ostringstream ss;
  ss << circuitPrefix;
  ss << gParams.dataAwareFlag;
  ss << "_";
  ss << gParams.routingPolicy;
  ss << ".out";
  pOut->print_to_file(ss.str());

  delete pOut;
  
  cout << "Total Solve Time:" << etime-stime << endl;
  print_stats();

  cout << "All done!\n";
  return 0;
}

#if 0
  gParams.is_greedy = 1;
  SMTOutput *pInitMap = new SMTOutput(pCircuit, pMachine);
  pInitMap->load_qubit_mapping(pCircuit->qx, pCircuit->qy);
  SMTOutput *pOut = fire_param_tune(test_sr_instance, pCircuit, pMachine, pInitMap, T2);
  SMTOutput *pOut = test_sr_instance(pCircuit, pMachine, pInitMap, T2);
  SMTOutput *pOutEJF = find_ejf_schedule(pCircuit, pMachine, pInitMap);
#endif

