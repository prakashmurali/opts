#include "main.h"


SMTSchedule::SMTSchedule(Circuit *pC, Machine *pM){
  pCircuit = pC;
  pMachine = pM;
  maxT = gParams.maxTimeSlot;
}

void SMTSchedule::_add_qubit_mapping_bounds(SolverType &opt, expr_vector &qx, expr_vector &qy){
  int cnt = 0;
  int i;
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  // row bound
  for(i=0; i<pC->nQubits; i++)
    opt.add(qx[i] >= 0 && qx[i] < pM->nRows); 
  cnt += pC->nQubits;
  // column bound
  for(i=0; i<pC->nQubits; i++)
    opt.add(qy[i] >= 0 && qy[i] < pM->nCols);
  cnt += pC->nQubits;
  // add unique mapping constraint
  int j;
  for(i=0; i<pC->nQubits; i++){
    for(j=i+1; j<pC->nQubits; j++){
      opt.add(qx[i] != qx[j] || qy[i] != qy[j]);
      cnt += 1;
    }
  }
  cout << "Qubit unique mapping:" << cnt << endl;
  gStats.cntMapping = cnt;
}

/**
 * Note: This is inaccurate. We cannot check mirror symmetry pairwise
 */
void SMTSchedule::_add_mirror_symmetry(SolverType &opt, expr_vector &qx, expr_vector &qy){
  Circuit *pC = pCircuit;
  int i,j;
  int machine_x_mid = gParams.machineRows-1; //this is mid*2 actually
  int machine_y_mid = gParams.machineCols-1;
  for(i=0; i<pC->nQubits; i++){
    for(j=i+1; j<pC->nQubits; j++){
      expr x_match = (qx[i] == qx[j]);
      expr y_match = (qy[i] == qy[j]);
      expr x_mid = (qx[i] + qx[j]); //this is again twice the mid
      expr y_mid = (qy[i] + qy[j]);
      expr cons_x = implies(x_match && (y_mid == machine_y_mid), qy[i] < qy[j]);
      expr cons_y = implies(y_match && (x_mid == machine_x_mid), qx[i] < qx[j]);
      opt.add(cons_x);
      opt.add(cons_y);
    }
  }
}

void SMTSchedule::_add_gate_time_bounds(SolverType &opt, expr_vector &g, expr_vector &d){
  int cnt = 0;
  int j;
  Circuit *pC = pCircuit;
  for(j=0; j<pC->nGates; j++)
    opt.add(g[j] > 0);
  cnt += pC->nGates;
  cout << "Gate time lower bound:" << cnt << endl;
}

void SMTSchedule::_add_coherence_aware_time_bounds(SolverType &opt, expr_vector &qx, expr_vector &qy, expr_vector &g, expr_vector &d){
  int cnt = 0;
  int j;
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  MachineProp *pProp = pM->pProp;
  Gate *pG;
  int R = gParams.machineRows;
  int C = gParams.machineCols;

  Qubit *pQ;
  for(j=0; j<pC->nGates; j++){
    pG = pC->pGates + j; 
    for(int k=0; k<pG->nBits; k++){
      Qubit *pQ = pG->pQubitList[k]; 
      int qid = pQ->id;
      for(int row=0; row<R; row++){
        for(int col=0; col<C; col++){
          //if the qubit is mapped to a particluar h/w qubit, respect its coherence time
          opt.add(implies((qx[qid] == row) && (qy[qid] == col), g[j] + d[j] <= (pProp->cohrTime[row][col])));
          cnt++;
        }
      }
    }
  }
  cout << "Coherence time upper bound:" << cnt << endl;
}

void SMTSchedule::_add_gate_dependency_constraints(SolverType &opt, expr_vector &g, expr_vector &d){
  int cnt = 0;
  int i,j;
  Circuit *pC = pCircuit; 
  Machine *pM = pMachine;

  Gate *pGate;
  Gate *pInGate;
  int id_cur_gate;
  int id_in_gate;

  for(i=0; i<pC->nGates; i++){
    pGate = pC->pGates + i;
    id_cur_gate = pGate->id;
    assert(id_cur_gate == i); 
    for(j=0; j<pGate->nIn; j++){
      pInGate = pGate->pInGates[j];
      id_in_gate = pInGate->id; 
      opt.add(g[id_cur_gate] > g[id_in_gate] + d[id_in_gate]); // note: g+d is exactly the finish time
      cnt++;
    }
  }
  cout << "Dependency:" << cnt << endl;
  gStats.cntDepend = cnt;
}

void SMTSchedule::_set_dummy_gate_bound(SolverType &opt, expr_vector &g, expr_vector &d, context &c, int maxTimeSlot){
  int cnt = 0;
  int j;
  Circuit *pC = pCircuit;
  for(j=0; j<pC->nGates; j++){
    opt.add(g[pC->nGates] >= g[j]+ d[j]);
    cnt++;
  }
  opt.add(g[pC->nGates] <= maxTimeSlot); //force all gates to finish within the allowed range of time
  cnt++;
  cout << "Dummy gate constraints:" << cnt << endl;
  assert(cnt == pC->nGates + 1);
}

void SMTSchedule::_add_single_qubit_gate_durations(SolverType &opt, expr_vector &d){
  int cnt = 0;
  int j;
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  MachineProp *pProp = pM->pProp;
  Gate *pG;
  for(j=0; j<pC->nGates; j++){
    pG = pC->pGates + j;
    if(pG->nBits == 1){
      opt.add(d[j] == pProp->gateTime[pG->type]-1); // machine duration for single qubit gate
      cnt++;
    }
  } 
  cout << "Single qubit gate duration:" << cnt << endl;
}

void SMTSchedule::_add_cnot_durations_no_data(SolverType &opt, expr_vector &qx, expr_vector &qy, expr_vector &d){
  int cnt = 0;
  int j;
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  MachineProp *pProp = pM->pProp;
  Gate *pG;
  for(j=0; j<pC->nGates; j++){
    pG = pC->pGates + j;
    if(pG->nBits == 2){
      Qubit *pQ1 = pG->pQubitList[0]; 
      Qubit *pQ2 = pG->pQubitList[1];
      int Q1id = pQ1->id;
      int Q2id = pQ2->id;
      expr distance = DISTANCE(qx[Q1id], qy[Q1id], qx[Q2id], qy[Q2id]);
      opt.add(d[j] == ( 2*((distance-1)*pProp->gateTime["SWAP"]) + pProp->gateTime[pG->type]-1) ); //2*swap time + cnot time
      cnt++;
    }
  }
  cout << "CNOT duration (data unaware):" << cnt << endl;
}

void SMTSchedule::_add_cnot_durations_rect_rsrv_data(SolverType &opt, expr_vector &qx, expr_vector &qy, expr_vector &d){
  int cnt = 0;
  int j;
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  assert(pM->nQubits == 16); //only ibmqx5
  MachineProp *pProp = pM->pProp;
  Gate *pG;
  int R = gParams.machineRows;
  int C = gParams.machineCols;

  for(j=0; j<pC->nGates; j++){
    pG = pC->pGates + j;
    if(pG->nBits == 2){
      Qubit *pQ1 = pG->pQubitList[0]; 
      Qubit *pQ2 = pG->pQubitList[1];
      int Q1id = pQ1->id;
      int Q2id = pQ2->id;
      //Machine size^2 constraints for every CNOT gate
      for(int row1 = 0; row1 < R; row1++){
        for(int col1 = 0; col1 < C; col1++){
          for(int row2 = 0; row2 < R; row2++){
            for(int col2 = 0; col2 < C; col2++){
              if(row1 == row2 && col1 == col2) continue;
              opt.add(implies(qx[Q1id] == row1 && qy[Q1id] == col1 && qx[Q2id] == row2 && qy[Q2id] == col2, \
                              d[j] == pProp->cnotRRTime[row1][col1][row2][col2]));
              cnt++;
            }
          }
        }
      }
    }  
  } 
  cout << "CNOT durations (data aware, rectangle reservation):" << cnt << "\n";
}



void SMTSchedule::_add_rectangle_reservation_constraints(SolverType &opt, context &c, expr_vector &qx, expr_vector &qy, expr_vector &g, expr_vector &d){
  int cnt = 0;
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  MachineProp *pProp = pM->pProp;
  Gate *pG1, *pG2;
  int i,j;
  int jIdx;
  int D = gParams.couplingThresh;
  IntVect cnotIdx;

  // create vars to maintain CNOT left and right corner
  expr_vector lx(c);
  expr_vector ly(c);
  expr_vector rx(c);
  expr_vector ry(c);
  for(j=0, jIdx=0; j<pC->nGates; j++){
    pG1 = pC->pGates + j;
    if(pG1->nBits != 2){
      cnotIdx.push_back(INVALID);
      continue;
    }
    stringstream min_x_name, max_x_name, min_y_name, max_y_name;
    min_x_name << "cnot_" << j << "_minx"; 
    max_x_name << "cnot_" << j << "_maxx";
    min_y_name << "cnot_" << j << "_miny";
    max_y_name << "cnot_" << j << "_maxy";
    lx.push_back(c.int_const(min_x_name.str().c_str())); 
    ly.push_back(c.int_const(max_x_name.str().c_str()));
    rx.push_back(c.int_const(min_y_name.str().c_str()));
    ry.push_back(c.int_const(max_y_name.str().c_str()));

    //add constraints to maintain these variables
    Qubit *pQc = pG1->pQubitList[0]; 
    Qubit *pQt = pG1->pQubitList[1];
    int cid = pQc->id;
    int tid = pQt->id;
    opt.add(MIN_MAX_COND(qx[cid], qx[tid], lx[jIdx], rx[jIdx]));
    opt.add(MIN_MAX_COND(qy[cid], qy[tid], ly[jIdx], ry[jIdx]));
    cnt += 2;
    cnotIdx.push_back(jIdx);
    jIdx++;
  }
  for(j=0; j<pC->nGates; j++){
    pG1 = pC->pGates + j;
    if(pG1->nBits != 2) continue;
    int g1 = cnotIdx[j];
    for(IntVectItr it = pC->cnotOverlaps[j].begin(); it != pC->cnotOverlaps[j].end(); it++){
      i = *it;
      pG2 = pC->pGates + i;
      expr gates_overlap_time = !((g[i] > g[j] + d[j]) || (g[j] > g[i] + d[i]));
      if(pG2->nBits == 1){
        // if single qubit gate, check if qubit is inside rectangle
        Qubit *pQk = pG2->pQubitList[0];
        int InQid = pQk->id;
        expr qubit_not_in_rectangle = !(POINT_IN_RECT(lx[g1]-D, ly[g1]-D, rx[g1]+D, ry[g1]+D, qx[InQid], qy[InQid]));
        opt.add(implies(gates_overlap_time, qubit_not_in_rectangle));
        cnt += 1;
      }else if(pG2->nBits == 2){
        int g2 = cnotIdx[i];
        // if two qubit gate, get both rectangles and check overlap
        if(gParams.is_greedy == 1){
          int is_overlap;
          int g1c, g1t;
          int g2c, g2t;
          int lx_g1, ly_g1;
          int rx_g1, ry_g1;
          int lx_g2, ly_g2;
          int rx_g2, ry_g2;
          g1c = (pG1->pQubitList[0])->id;
          g1t = (pG1->pQubitList[1])->id;
          g2c = (pG2->pQubitList[0])->id;
          g2t = (pG2->pQubitList[1])->id;
          lx_g1 = MIN(pC->qx[g1c], pC->qx[g1t]);
          ly_g1 = MIN(pC->qy[g1c], pC->qy[g1t]);
          rx_g1 = MAX(pC->qx[g1c], pC->qx[g1t]);
          ry_g1 = MAX(pC->qy[g1c], pC->qy[g1t]);
          lx_g2 = MIN(pC->qx[g2c], pC->qx[g2t]);
          ly_g2 = MIN(pC->qy[g2c], pC->qy[g2t]);
          rx_g2 = MAX(pC->qx[g2c], pC->qx[g2t]);
          ry_g2 = MAX(pC->qy[g2c], pC->qy[g2t]);
          is_overlap = !(RECT_NOT_OVERLAP(lx_g1, ly_g1, rx_g1, ry_g1, lx_g2, ly_g2, rx_g2, ry_g2));
          if(is_overlap){
            opt.add(!gates_overlap_time);
          }
        }else{
          if(D > 0){
            expr rectangles_not_overlap = (RECT_NOT_OVERLAP(lx[g1]-D, ly[g1]-D, rx[g1]+D, ry[g1]+D, lx[g2], ly[g2], rx[g2], ry[g2]));
            opt.add(implies(gates_overlap_time, rectangles_not_overlap));
          }else{
            expr rectangles_not_overlap = (RECT_NOT_OVERLAP(lx[g1], ly[g1], rx[g1], ry[g1], lx[g2], ly[g2], rx[g2], ry[g2]));
            opt.add(implies(gates_overlap_time, rectangles_not_overlap));
          }
        }
        cnt += 1;
      }
    }
  }
  cout << "Routing (rectangle reservation):" << cnt << endl;
  gStats.cntRouting = cnt;
}

void SMTSchedule::_add_cnot_durations_one_bend_data(SolverType &opt, context &c, expr_vector &qx, expr_vector &qy, expr_vector &d){
  int cnt = 0;
  int j;
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  assert(pM->nQubits == 16); //only ibmqx5
  MachineProp *pProp = pM->pProp;
  Gate *pG;
  int R = gParams.machineRows;
  int C = gParams.machineCols;

  expr_vector jx(c);
  expr_vector jy(c);
  int jIdx=0;
  for(j=0; j<pC->nGates; j++){
    pG = pC->pGates + j;
    if(pG->nBits != 2) continue;

    Qubit *pQ1 = pG->pQubitList[0]; 
    Qubit *pQ2 = pG->pQubitList[1];
    int Q1id = pQ1->id;
    int Q2id = pQ2->id;

    stringstream jx_name, jy_name;
    jx_name << "cnot1b_jx_" << j;
    jy_name << "cnot1b_jy_" << j;
    jx.push_back(c.int_const(jx_name.str().c_str()));
    jy.push_back(c.int_const(jy_name.str().c_str()));
    for(int row1 = 0; row1 < R; row1++){
      for(int col1 = 0; col1 < C; col1++){
        for(int row2 = 0; row2 < R; row2++){
          for(int col2 = 0; col2 < C; col2++){
            if(row1 == row2 && col1 == col2) continue;

              int j1x, j1y, j2x, j2y, j1Time, j2Time;
              // first junction
              j1x = pProp->cnotOneBendTime[row1][col1][row2][col2][0][0];
              j1y = pProp->cnotOneBendTime[row1][col1][row2][col2][0][1];
              j1Time = pProp->cnotOneBendTime[row1][col1][row2][col2][0][2];
              // second junction
              j2x = pProp->cnotOneBendTime[row1][col1][row2][col2][1][0];
              j2y = pProp->cnotOneBendTime[row1][col1][row2][col2][1][1];
              j2Time = pProp->cnotOneBendTime[row1][col1][row2][col2][1][2];
              
              expr is_this_cnot = (qx[Q1id] == row1 && qy[Q1id] == col1 && qx[Q2id] == row2 && qy[Q2id] == col2);
              expr is_junct1 = (jx[jIdx] == j1x && jy[jIdx] == j1y);
              expr is_junct2 = (jx[jIdx] == j2x && jy[jIdx] == j2y);
 
              if(j1Time == -1)
                opt.add(implies(is_this_cnot, !is_junct1));
              else
               opt.add(implies(is_this_cnot && is_junct1, d[j] == j1Time));
              
              if(j2Time == -1)
                opt.add(implies(is_this_cnot, !is_junct2));
              else
               opt.add(implies(is_this_cnot && is_junct2, d[j] == j2Time));
              cnt += 2;
          }
        }
      }
    }
    jIdx++;
  }
  cout << "CNOT durations (data aware, one bend):" << cnt << endl;
}

void SMTSchedule::_add_1bend_reservation_constraints(SolverType &opt, context &c, expr_vector &qx, expr_vector &qy, expr_vector &g, expr_vector &d){
  int cnt = 0;
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  MachineProp *pProp = pM->pProp;
  Gate *pG1, *pG2;
  int g1, g2;
  int i,j;
  int D = gParams.couplingThresh;
  IntVect cnotIdx;

  // create vars to pick junction for a cnot, and maintain c-j and j-t rectangle corners
  expr_vector CJ_lx(c);
  expr_vector CJ_ly(c);
  expr_vector CJ_rx(c);
  expr_vector CJ_ry(c);
  expr_vector JT_lx(c);
  expr_vector JT_ly(c);
  expr_vector JT_rx(c);
  expr_vector JT_ry(c);
  expr_vector jx(c);
  expr_vector jy(c);
  int jIdx;
  for(j=0, jIdx=0; j<pC->nGates; j++){
    pG1 = pC->pGates + j;
    if(pG1->nBits != 2){
      cnotIdx.push_back(INVALID);
      continue;
    }
    stringstream CJ_lx_n, CJ_ly_n, CJ_rx_n, CJ_ry_n, JT_lx_n, JT_ly_n, JT_rx_n, JT_ry_n, jx_name, jy_name;
    CJ_lx_n << "cnot1b_cj_lx_" << j;
    CJ_ly_n << "cnot1b_cj_ly_" << j;
    CJ_rx_n << "cnot1b_cj_rx_" << j;
    CJ_ry_n << "cnot1b_cj_ry_" << j;
    JT_lx_n << "cnot1b_jt_lx_" << j;
    JT_ly_n << "cnot1b_jt_ly_" << j;
    JT_rx_n << "cnot1b_jt_rx_" << j;
    JT_ry_n << "cnot1b_jt_ry_" << j;
    jx_name << "cnot1b_jx_" << j;
    jy_name << "cnot1b_jy_" << j;
    CJ_lx.push_back(c.int_const(CJ_lx_n.str().c_str())); 
    CJ_ly.push_back(c.int_const(CJ_ly_n.str().c_str())); 
    CJ_rx.push_back(c.int_const(CJ_rx_n.str().c_str())); 
    CJ_ry.push_back(c.int_const(CJ_ry_n.str().c_str())); 
    JT_lx.push_back(c.int_const(JT_lx_n.str().c_str())); 
    JT_ly.push_back(c.int_const(JT_ly_n.str().c_str())); 
    JT_rx.push_back(c.int_const(JT_rx_n.str().c_str())); 
    JT_ry.push_back(c.int_const(JT_ry_n.str().c_str())); 
    jx.push_back(c.int_const(jx_name.str().c_str()));
    jy.push_back(c.int_const(jy_name.str().c_str()));

    //add constraints to maintain these variables
    Qubit *pQc = pG1->pQubitList[0]; 
    Qubit *pQt = pG1->pQubitList[1];
    int cid = pQc->id;
    int tid = pQt->id;
    opt.add(MIN_MAX_COND(qx[cid], jx[jIdx], CJ_lx[jIdx], CJ_rx[jIdx]));
    opt.add(MIN_MAX_COND(qy[cid], jy[jIdx], CJ_ly[jIdx], CJ_ry[jIdx]));
    opt.add(MIN_MAX_COND(jx[jIdx], qx[tid], JT_lx[jIdx], JT_rx[jIdx]));
    opt.add(MIN_MAX_COND(jy[jIdx], qy[tid], JT_ly[jIdx], JT_ry[jIdx]));
    opt.add((jx[jIdx] == qx[cid] && jy[jIdx] == qy[tid]) || (jx[jIdx] == qx[tid] && jy[jIdx] == qy[cid]));
    cnt += 5;
    cnotIdx.push_back(jIdx);
    jIdx++;
  }

  for(j=0; j<pC->nGates; j++){
    pG1 = pC->pGates + j;
    if(pG1->nBits != 2) continue;
    int g1 = cnotIdx[j];
    for(IntVectItr it = pC->cnotOverlaps[j].begin(); it != pC->cnotOverlaps[j].end(); it++){
      i = *it;
      pG2 = pC->pGates + i;
      expr gates_dont_overlap_time = ((g[i] > g[j] + d[j]) || (g[j] > g[i] + d[i]));
      expr gates_overlap_time = !gates_dont_overlap_time;
      if(pG2->nBits == 1){
        // if single qubit gate, check if qubit is inside cj or jt rectangle
        Qubit *pQk = pG2->pQubitList[0];
        int InQid = pQk->id;
        expr qubit_in_rectangle_cj = (POINT_IN_RECT(CJ_lx[g1]-D, CJ_ly[g1]-D, CJ_rx[g1]+D, CJ_ry[g1]+D, qx[InQid], qy[InQid]));
        expr qubit_in_rectangle_jt = (POINT_IN_RECT(JT_lx[g1]-D, JT_ly[g1]-D, JT_rx[g1]+D, JT_ry[g1]+D, qx[InQid], qy[InQid]));
        //opt.add(implies(qubit_in_rectangle_cj || qubit_in_rectangle_jt, gates_dont_overlap_time));
        opt.add(implies(gates_overlap_time, (!qubit_in_rectangle_cj && !qubit_in_rectangle_jt)));
        cnt += 1;
      }else if(pG2->nBits == 2){
        int g2 = cnotIdx[i];
        // if two qubit gate, get both rectangles and check overlap
        if(D > 0){
          expr nocj_cj = (RECT_NOT_OVERLAP(CJ_lx[g1]-D, CJ_ly[g1]-D, CJ_rx[g1]+D, CJ_ry[g1]+D, CJ_lx[g2], CJ_ly[g2], CJ_rx[g2], CJ_ry[g2]));
          expr nocj_jt = (RECT_NOT_OVERLAP(CJ_lx[g1]-D, CJ_ly[g1]-D, CJ_rx[g1]+D, CJ_ry[g1]+D, JT_lx[g2], JT_ly[g2], JT_rx[g2], JT_ry[g2]));
          expr nojt_cj = (RECT_NOT_OVERLAP(JT_lx[g1]-D, JT_ly[g1]-D, JT_rx[g1]+D, JT_ry[g1]+D, CJ_lx[g2], CJ_ly[g2], CJ_rx[g2], CJ_ry[g2]));
          expr nojt_jt = (RECT_NOT_OVERLAP(JT_lx[g1]-D, JT_ly[g1]-D, JT_rx[g1]+D, JT_ry[g1]+D, JT_lx[g2], JT_ly[g2], JT_rx[g2], JT_ry[g2]));
          expr spatial_no_overlap = (nocj_cj && nocj_jt && nojt_cj && nojt_jt);
          opt.add(implies(gates_overlap_time, spatial_no_overlap));
        }else{
          expr nocj_cj = (RECT_NOT_OVERLAP(CJ_lx[g1], CJ_ly[g1], CJ_rx[g1], CJ_ry[g1], CJ_lx[g2], CJ_ly[g2], CJ_rx[g2], CJ_ry[g2]));
          expr nocj_jt = (RECT_NOT_OVERLAP(CJ_lx[g1], CJ_ly[g1], CJ_rx[g1], CJ_ry[g1], JT_lx[g2], JT_ly[g2], JT_rx[g2], JT_ry[g2]));
          expr nojt_cj = (RECT_NOT_OVERLAP(JT_lx[g1], JT_ly[g1], JT_rx[g1], JT_ry[g1], CJ_lx[g2], CJ_ly[g2], CJ_rx[g2], CJ_ry[g2]));
          expr nojt_jt = (RECT_NOT_OVERLAP(JT_lx[g1], JT_ly[g1], JT_rx[g1], JT_ry[g1], JT_lx[g2], JT_ly[g2], JT_rx[g2], JT_ry[g2]));
          expr spatial_no_overlap = (nocj_cj && nocj_jt && nojt_cj && nojt_jt);
          opt.add(implies(gates_overlap_time, spatial_no_overlap));
 
        }
          //cout << implies(gates_overlap_time, spatial_no_overlap) << "\n\n";
        cnt += 1;
      }
    }
  }
  cout << "Routing (one bend):" << cnt << endl;
  gStats.cntRouting = cnt;  
}

/**
 * Init for binary search version of compute_schedule
 */
void SMTSchedule::create_optimization_instance(context &c, SolverType &opt, int routing_flag){
  assert(pCircuit->nQubits <= pMachine->nQubits);
  cout << "Creating optimization instance...\n";
  //qubit mapping vars
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

  _add_qubit_mapping_bounds(opt, qx, qy);
  _add_gate_time_bounds(opt, g, d); // only adds a 0 lower bound
  _add_single_qubit_gate_durations(opt, d);
  _add_gate_dependency_constraints(opt, g, d);

  //CNOT Time constraints
  if(gParams.dataAwareFlag == FALSE){
    _add_cnot_durations_no_data(opt, qx, qy, d);  //Cnot time
  }else{
    if(gParams.routingPolicy == RECTANGLE_RSRV){ _add_cnot_durations_rect_rsrv_data(opt, qx, qy, d); }       //Cnot time RR
    else if(gParams.routingPolicy == ONE_BEND_RSRV){ _add_cnot_durations_one_bend_data(opt, c, qx, qy, d); } //Cnot time 1-bend
    else { assert(0); }
  } 
 
  //Coherence window constraint
  if(gParams.dataAwareFlag == TRUE){
    _add_coherence_aware_time_bounds(opt, qx, qy, g, d); //T2 based constraints
  }
  
  if(routing_flag == TRUE){
    if(gParams.routingPolicy == RECTANGLE_RSRV) 
      _add_rectangle_reservation_constraints(opt, c, qx, qy, g, d);
    else if(gParams.routingPolicy == ONE_BEND_RSRV)
      _add_1bend_reservation_constraints(opt, c, qx, qy, g, d);
    else {assert (0);}
  }else;

  cout << "Created instance\n";  
}

/**
 * Check routing for binary search version of compute_schedule
 */
SMTOutput *SMTSchedule::check_instance(context &c, SolverType &opt, int maxTimeSlot){
  //gate time vars
  cout << "Checking instance with T=" << maxTimeSlot << endl;

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
    cout << "SAT\nFound a schedule with execution duration:" << m.eval(g[pCircuit->nGates]) << "\n"; 
    pOut->result = IS_SAT;
    _copy_qubit_mapping(pOut, m, qx, qy);
    _copy_gate_times(pOut, m, g, d);
    if(gParams.routingPolicy == ONE_BEND_RSRV){
      _copy_cnot_route(pOut, m, c);
    }
  }else{
    pOut->result = IS_UNSAT;
    cout << "UNSAT\n";
  }
  return(pOut);
}

/**
 * Performs mapping, schedulimg routing
 * Policies: RR, 1BP
 * Sat Checking/Optimize, one shot
 */
SMTOutput* SMTSchedule::compute_schedule(){
  context c;
  SolverType opt(c);
  
  assert(pCircuit->nQubits <= pMachine->nQubits);

  //qubit mapping vars
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

  cout << "Adding dummy gate\n";
  std::stringstream g_name;
  g_name << "g_dummy";
  g.push_back( c.int_const(g_name.str().c_str()) );

  // duration vars
  expr_vector d(c);
  for(j=0; j<pCircuit->nGates; j++){
    std::stringstream d_name;
    d_name << "d_" << pCircuit->pGates[j].id;
    d.push_back( c.int_const(d_name.str().c_str()) );
  }


  //Add Constraints

  _add_qubit_mapping_bounds(opt, qx, qy);
  _add_single_qubit_gate_durations(opt, d);

  if(gParams.dataAwareFlag == 0){
    _add_gate_time_bounds(opt, g, d); //maxT
    _add_cnot_durations_no_data(opt, qx, qy, d);  //Cnot time
  }else{
    _add_coherence_aware_time_bounds(opt, qx, qy, g, d); //T2 based constraints
    if(gParams.routingPolicy == RECTANGLE_RSRV){ _add_cnot_durations_rect_rsrv_data(opt, qx, qy, d);}       //Cnot time RR
    else if(gParams.routingPolicy == ONE_BEND_RSRV){ _add_cnot_durations_one_bend_data(opt, c, qx, qy, d);} //Cnot time 1-bend
  }

  _add_gate_dependency_constraints(opt, g, d);
  _set_dummy_gate_bound(opt, g, d, c, gParams.maxTimeSlot);
  if(gParams.routingPolicy == RECTANGLE_RSRV) 
    _add_rectangle_reservation_constraints(opt, c, qx, qy, g, d);
  else if(gParams.routingPolicy == ONE_BEND_RSRV)
    _add_1bend_reservation_constraints(opt, c, qx, qy, g, d);


  cout << "Solving...\n";
 
  //SolverType::handle h = opt.minimize(g[pCircuit->nGates]);

  SMTOutput *pOut = new SMTOutput(pCircuit, pMachine);
  if(opt.check() == sat){
    model m = opt.get_model();
    _print_qubit_mapping(m, qx, qy);
    cout << "Dummy gate time:" << m.eval(g[pCircuit->nGates]) << "\n"; 
    pOut->result = IS_SAT;
    _copy_qubit_mapping(pOut, m, qx, qy);
    _copy_gate_times(pOut, m, g, d);
    if(gParams.routingPolicy == ONE_BEND_RSRV){
      _copy_cnot_route(pOut, m, c);
    }
  }else{
    pOut->result = IS_UNSAT;
  }
  
  //cout << opt.statistics();
  return(pOut);
}




