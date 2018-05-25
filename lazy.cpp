#include "main.h"

/**
 * This file supports two lazy routing policies.
 * Routing constraints are ommited from the solver and a SAT check is performed.
 * If SAT, then
 *    fix/operate on routing conflicts, report outcome
 * If UNSAT,
 *    report UNSAT
 *
 * For handling routing conflicts, we have two policies:
 * Greedy Route - 
 *    Find the earliest conflict. 
 *    Fix the first gate in this conflict set, 
 *    move conflicting gates and their dependents if needed (based on slack with in gates)
 *    Now find the set of conflicts again and iterate.
 * CEGAR Route - 
 *    Find set of all conflicts.
 *    Create clauses to block these conflicts (in terms of gate overlap times, no geometry checks)
 *    Pass these clauses to solver, check SAT.
 *    If SAT, check conflicts and repeat until we get SAT and no conflicts.
 *    If UNSAT, it is possible that we have create too many harsh conflict clauses (since we are constraining based on time only)
 *    for each conflict we have two options:
 *      a) add a time based conflict cluase (don't overlap in  time)
 *      b) add the full conflict clause (dont overlap in time if you overlap in space)
 *    option a) UNSAT - we cant be sure if we are overconstraining
 *    option b) UNSAT - means UNSAT
 *
 *    Advantage: Instead of presenting all routing constraints at once, we present them as needed, avoidnig the quadratic cost upfront.
 *            
 * For both policies the instance is created as in compute_schedule, but avoiding the routing constraints.
 * Test #1 - Check if this skeleton instance works fast 
 * next TODO
 * write function to check RR routing conflicts
 * report count
 */


/**
 * Init for lazy routing
 */
void SMTSchedule::lr_create_optimization_instance(context &c, SolverType &opt){
  create_optimization_instance(c, opt, FALSE);
}

SMTOutput *SMTSchedule::lr_check_current_instance(context &c, SolverType &opt, int maxTimeSlot){
  cout << "Checking instance with T=" << maxTimeSlot << endl;
  assert(gParams.routingPolicy == RECTANGLE_RSRV);

  expr_vector qx(c);
  expr_vector qy(c);
  int i;
  for(i=0; i<pCircuit->nQubits; i++){
    std::stringstream qx_name;
    qx_name << "qx_" << pCircuit->pQubits[i].id;
    qx.push_back( c.int_const(qx_name.str().c_str()) );

    std::stringstream qy_name;
    qy_name << "qy_" << pCircuit->pQubits[i].id;
    qy.push_back( c.int_const(qy_name.str().c_str()) );
  }

  //gate time vars
  expr_vector g(c);
  int j;
  for(j=0; j<pCircuit->nGates; j++){
    std::stringstream g_name;
    g_name << "g_" << pCircuit->pGates[j].id;
    g.push_back( c.int_const(g_name.str().c_str()) );
  }

  // duration vars
  expr_vector d(c);
  for(j=0; j<pCircuit->nGates; j++){
    std::stringstream d_name;
    d_name << "d_" << pCircuit->pGates[j].id;
    d.push_back( c.int_const(d_name.str().c_str()) );
  }

  //adding dummy gate
  std::stringstream g_name;
  g_name << "g_dummy";
  g.push_back( c.int_const(g_name.str().c_str()) );

  _set_dummy_gate_bound(opt, g, d, c, maxTimeSlot); //global max time slot added

  SMTOutput *pOut = new SMTOutput(pCircuit, pMachine);
  if(opt.check() == sat){
    model m = opt.get_model();
    cout << "SAT\nDummy gate time:" << m.eval(g[pCircuit->nGates]) << "\n"; 
    pOut->result = IS_SAT;
    _copy_qubit_mapping(pOut, m, qx, qy);
    _copy_gate_times(pOut, m, g, d);
  }else{
    pOut->result = IS_UNSAT;
    cout << "UNSAT\n";
  }
  return(pOut);
}

int SMTOutput::_check_routing_conflict(int g1, int g2, int c1, int t1, int c2, int t2){
  //assumming RR
  assert(gParams.routingPolicy == RECTANGLE_RSRV);

  int not_overlap_time = (g[g1] >= g[g2] + d[g2]) || (g[g2] >= g[g1] + d[g1]);
  if(not_overlap_time) return FALSE;

  int lX1, lY1;
  int rX1, rY1;
  lX1 = MIN(qx[c1], qx[t1]);
  rX1 = MAX(qx[c1], qx[t1]);
  lY1 = MIN(qy[c1], qy[t1]);
  rY1 = MAX(qy[c1], qy[t1]);

  int lX2, lY2;
  int rX2, rY2;
  lX2 = MIN(qx[c2], qx[t2]);
  rX2 = MAX(qx[c2], qx[t2]);
  lY2 = MIN(qy[c2], qy[t2]);
  rY2 = MAX(qy[c2], qy[t2]);
  int not_overlap_space = RECT_NOT_OVERLAP(lX1, lY1, rX1, rY1, lX2, lY2, rX2, rY2); 
  if(not_overlap_space) 
    return FALSE;
  else{
#if 0
    cout << "Conflict\n";
    cout << g1 << "," << g[g1] << " and " << g2 << "," << g[g2] << "\n";
    cout << lX1 << "," <<  lY1 << "  " << rX1 << "," <<  rY1 << "\n";
    cout << lX2 << "," <<  lY2 << "  " << rX2 << "," <<  rY2 << "\n";
    cout << g[g1] << "," << d[g1] << "  " << g[g2] << "," << d[g2] << "\n";
#endif
    return(TRUE); 
  }
}


int SMTSchedule::_lr_cegar_process_routing_conflicts(context &c, SolverType &opt, SMTOutput *pOut){
  //gate time vars
  expr_vector g(c);
  int j;
  for(j=0; j<pCircuit->nGates; j++){
    std::stringstream g_name;
    g_name << "g_" << pCircuit->pGates[j].id;
    g.push_back( c.int_const(g_name.str().c_str()) );
  }

  // duration vars
  expr_vector d(c);
  for(j=0; j<pCircuit->nGates; j++){
    std::stringstream d_name;
    d_name << "d_" << pCircuit->pGates[j].id;
    d.push_back( c.int_const(d_name.str().c_str()) );
  }

  //take qubit mapping, look for cnot pairs, check
  Gate *pG1;
  Qubit *pC1, *pT1;
  Gate *pG2;
  Qubit *pC2, *pT2;
  int cnt=0;
  for(j=0; j<pCircuit->nGates; j++){
    pG1 = pCircuit->pGates + j;
    if(pG1->nBits != 2) continue;
    int cid1, tid1;
    pC1 = pG1->pQubitList[0];
    pT1 = pG1->pQubitList[1];
    cid1 = pC1->id;
    tid1 = pT1->id;

    for(int k=j+1; k<pCircuit->nGates; k++){
      pG2 = pCircuit->pGates + k;
      if(pG2->nBits != 2) continue;
      int cid2, tid2;
      pC2 = pG2->pQubitList[0];
      pT2 = pG2->pQubitList[1];
      cid2 = pC2->id;
      tid2 = pT2->id;
      int conflict = pOut->_check_routing_conflict(j, k, cid1, tid1, cid2, tid2);
      cnt += conflict;
      if(conflict){
        expr not_overlap_time = ((g[j] > g[k] + d[k]) || (g[k] > g[j] + d[j]));
        opt.add(not_overlap_time);
      }
    }
  }
  return (cnt);
}

SMTOutput* SMTSchedule::lr_cegar_solve(context &c, SolverType &opt, int maxTimeSlot){
  lr_create_optimization_instance(c, opt);
  int flag;
  while(1){
    SMTOutput *pOut = lr_check_current_instance(c, opt, maxTimeSlot);
    if(pOut->result == IS_SAT){ 
      int num_conflicts = _lr_cegar_process_routing_conflicts(c, opt, pOut);
      cout << "**Conflicts: " << num_conflicts << endl;
      if(num_conflicts > 0){
        continue;
      }else{
        cout << "CEGAR-SAT!" << endl;
        return(pOut);
      }
    }else if(pOut->result == IS_UNSAT){
      cout << "CEGAR-UNSAT!" << endl;
      return(pOut);
    }
  }
}

int SMTOutput::_num_conflicts(){
  //take qubit mapping, look for cnot pairs, check
  Gate *pG1;
  Qubit *pC1, *pT1;
  Gate *pG2;
  Qubit *pC2, *pT2;
  int cnt=0;
  int j;
  for(j=0; j<pCircuit->nGates; j++){
    pG1 = pCircuit->pGates + j;
    if(pG1->nBits != 2) continue;
    int cid1, tid1;
    pC1 = pG1->pQubitList[0];
    pT1 = pG1->pQubitList[1];
    cid1 = pC1->id;
    tid1 = pT1->id;

    for(int k=j+1; k<pCircuit->nGates; k++){
      pG2 = pCircuit->pGates + k;
      if(pG2->nBits != 2) continue;
      int cid2, tid2;
      pC2 = pG2->pQubitList[0];
      pT2 = pG2->pQubitList[1];
      cid2 = pC2->id;
      tid2 = pT2->id;
      int conflict = _check_routing_conflict(j, k, cid1, tid1, cid2, tid2);
      cnt += conflict;
    }
  }
  return cnt;
}

int SMTOutput::_last_gate_time(){
  Gate *pG1;
  int j;
  int max_time = -1;
  int index = 0;
  for(j=0; j<pCircuit->nGates; j++){
    int end_time = g[j] + d[j];
    if(end_time > max_time){
      max_time = end_time;
      index = j;
    }
  }
  cout << "Last index:" << index << "\n";
  return max_time;
}

int SMTOutput::_fix_earliest_conflict(){
  //take qubit mapping, look for cnot pairs, check
  Gate *pG1;
  Qubit *pC1, *pT1;
  Gate *pG2;
  Qubit *pC2, *pT2;
  int cnt=0;
  int j;
  int min_time = MILLION;
  int min_gate_id;

  for(j=0; j<pCircuit->nGates; j++){
    pG1 = pCircuit->pGates + j;
    if(pG1->nBits != 2) continue;
    int cid1, tid1;
    pC1 = pG1->pQubitList[0];
    pT1 = pG1->pQubitList[1];
    cid1 = pC1->id;
    tid1 = pT1->id;

    for(int k=j+1; k<pCircuit->nGates; k++){
      pG2 = pCircuit->pGates + k;
      if(pG2->nBits != 2) continue;
      int cid2, tid2;
      pC2 = pG2->pQubitList[0];
      pT2 = pG2->pQubitList[1];
      cid2 = pC2->id;
      tid2 = pT2->id;
      int conflict = _check_routing_conflict(j, k, cid1, tid1, cid2, tid2);
      cnt += conflict;
      if(conflict){
        if(g[j] < min_time){
          min_time = g[j];
          min_gate_id = j;
        }
        if(g[k] < min_time){
          min_time = g[k];
          min_gate_id = k;
        }
      }
    }
  }

  if(cnt == 0) return 0;

  pG1 = pCircuit->pGates + min_gate_id;
  int cid1, tid1;
  pC1 = pG1->pQubitList[0];
  pT1 = pG1->pQubitList[1];
  cid1 = pC1->id;
  tid1 = pT1->id;

  IntVect conflictSet; 
  for(int k=0; k<pCircuit->nGates; k++){
    if(k == min_gate_id) continue;
    pG2 = pCircuit->pGates + k;
    if(pG2->nBits != 2) continue;
    int cid2, tid2;
    pC2 = pG2->pQubitList[0];
    pT2 = pG2->pQubitList[1];
    cid2 = pC2->id;
    tid2 = pT2->id;
    int conflict = _check_routing_conflict(min_gate_id, k, cid1, tid1, cid2, tid2);
    if(conflict){
      conflictSet.push_back(k);
      break; //take only 1 conflict at a time
    }
  }


  for(IntVectItr itr = conflictSet.begin(); itr != conflictSet.end(); itr++){
    int gate_id = *itr;
    g[gate_id] = MAX(g[gate_id], g[min_gate_id] + d[min_gate_id] + 1);
    for(IntVectItr itr2 = pCircuit->gateDepends[gate_id].begin(); itr2 != pCircuit->gateDepends[gate_id].end(); itr2++){
      int dgate_id = *itr2;
      Gate *pGate = pCircuit->pGates + dgate_id;
      assert(pGate->id == dgate_id);

      for(j=0; j<pGate->nIn; j++){
        Gate *pInGate = pGate->pInGates[j];
        int id_in_gate = pInGate->id; 
        if(g[dgate_id] <= g[id_in_gate] + d[id_in_gate]){
          g[dgate_id] = MAX(g[dgate_id], g[id_in_gate] + d[id_in_gate] + 1);
        }
      }
    }
  }

  return cnt;
}

SMTOutput* SMTSchedule::lr_greedy_solve(context &c, SolverType &opt, int maxTimeSlot){
  lr_create_optimization_instance(c, opt);
  SMTOutput *pOut = lr_check_current_instance(c, opt, maxTimeSlot);
  if(pOut->result == IS_UNSAT) return pOut;  

  //now take this out and fix all conflicts in it.
  //find new critical path time 
  int num_conflicts;
  while(1){
    pOut->_fix_earliest_conflict();
    num_conflicts = pOut->_num_conflicts();
    cout << "Num conflicts:" << num_conflicts << " Last gate:" << pOut->_last_gate_time() << "\n";
    if(num_conflicts == 0) break;
  }
  pOut->check_sanity();
  return pOut;
}
