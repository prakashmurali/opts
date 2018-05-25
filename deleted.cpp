#if 0

void SMTSchedule::_add_coupling_constraint(solver &opt, expr_vector &qx, expr_vector &qy, expr_vector &g){
  // if two gates overlap, then they should be distance k away
  int i;
  int j;
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  MachineProp *pProp = pM->pProp;
  map<string, int> *gateTime = pProp->gateTime;
  
  Gate *pG1, pG2;
  for(i=0; i<pC->nGates; i++){
    pG1 = pC->pGates + i;
    for(j=i+1; j<pC->nGates; j++){
      pG2 = pC->pGates + j;
      expr g1_start = g[i];
      expr g2_start = g[j];
      expr g1_end = g[i] + gateTime[pG1->type] - 1; 
      expr g2_end = g[j] + gateTime[pG2->type] - 1;
      // not overlap: g2 starts after g1 ends or g1 starts after g2 ends
      expr not_overlap = (g1_start > g2_end || g2_start > g1_end);
      
      if(pG1->nBits == 1 && pG2->nBits == 1){
        //expr d_x = 
        //expr is_k_far_away = 
      }
    }
  } 
}

#if 0
    for(i=0; i<pC->nGates; i++){
      if(i==j) continue; // skip the CNOT gate we are looking at now
      // check if G1 and G2 share qubits, if they do, they wont overlap because of dependency constraints
      int flag_skip = 0;
      for(int k=0; k<pG2->nBits; k++){
        Qubit *pQk = pG2->pQubitList[k];
        if(pQk->id == cQid || pQk->id == tQid){
          flag_skip = 1;
          break;
        }
      }
      if(flag_skip == 1) continue;
      
      pG2 = pC->pGates + i;
      for(int k=0; k<pG2->nBits; k++){
        Qubit *pQk = pG2->pQubitList[k];
        int InQid = pQk->id;
        expr qubit_is_inside_rectangle = (qx[InQid] <= qx[cQid] || qx[InQid] <= qx[tQid]) && \
                                         (qx[InQid] >= qx[cQid] || qx[InQid] >= qx[tQid]) && \
                                         (qy[InQid] <= qy[cQid] || qy[InQid] <= qy[tQid]) && \
                                         (qy[InQid] >= qy[cQid] || qy[InQid] >= qy[tQid]); 
 
        expr gates_dont_overlap = ((g[i] > g[j] + d[j]) || (g[j] > g[i] + d[i]));
        opt.add(implies(qubit_is_inside_rectangle, gates_dont_overlap));
        cnt++;
      }
    } 
#endif      


#endif

#if 0
  int stime, etime;
  try{
    context c;
    SolverType opt(c);

    SMTSchedule *pSolver = new SMTSchedule(pCircuit, pMachine);
    stime = MyGetTime();
    //SMTOutput *pOut = pSolver->compute_schedule();

    pSolver->create_optimization_instance(c, opt);
    SMTOutput *pOut = pSolver->check_instance(c, opt, gParams.maxTimeSlot);

    etime = MyGetTime();
    cout << "Solve time:" << (etime-stime) << "\n";
    if(pOut->result == IS_SAT){
      cout << "SATSAT\n";
      //pOut->check_sanity();
      //pOut->print_to_file(argv[3]);
    }else{
      cout << "UNSATUNSAT\n";
    }
  }
  catch(const z3::exception &e){
    cerr << e;
  }
#endif

#if 0
  //near_optimal_search(pCircuit, pMachine, 1.1);
  //solve_mapping(pCircuit, pMachine);
  //_test_an_instance(pCircuit, pMachine, gParams.maxTimeSlot);
  /*
  SMTOutput *pOut = _test_lr_instance(pCircuit, pMachine, gParams.maxTimeSlot);
  if(pOut->result == IS_SAT){
    cout << "SATSAT\n";
    pOut->check_sanity();
    pOut->print_to_file(argv[3]);
  }
  */
#endif 
