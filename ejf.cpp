#include "main.h"

EJFSchedule::EJFSchedule(Circuit *pC, Machine *pM, SMTOutput *pOut){
  pCircuit = pC;
  pMachine = pM;
  pInitMap = pOut;
}

void EJFSchedule::_compute_durations(){
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  MachineProp *pProp = pM->pProp;
  SMTOutput *pMap = pInitMap;
  
  Gate *pG;
  int id_cur_gate;

  int i;
  for(i=0; i<pC->nGates; i++){
    pG = pC->pGates + i;
    id_cur_gate = pG->id;
    
    if(pG->nBits == 1){
      duration[i] = pProp->gateTime[pG->type]-1; 
    }else{
      Qubit *pQ1 = pG->pQubitList[0]; 
      Qubit *pQ2 = pG->pQubitList[1];
      int Q1id = pQ1->id;
      int Q2id = pQ2->id;
      int distance = abs(pMap->qx[Q1id] - pMap->qx[Q2id]) + abs(pMap->qy[Q1id] - pMap->qy[Q2id]);
      duration[i] = 2*((distance-1)*pProp->gateTime["SWAP"]) + pProp->gateTime[pG->type] - 1;
    }
  }
}

void EJFSchedule::_compute_cnot_conflicts(){
  //compute if two cnots have spatial RR conflict
  int j;
  Gate *pG1, *pG2;
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  SMTOutput *pMap = pInitMap;

  for(j=0; j<pC->nGates; j++){
    pG1 = pC->pGates + j;
    if(pG1->nBits != 2) continue;
    cnotOverlaps2way[j] = IntVect();
  }
  for(j=0; j<pC->nGates; j++){
    pG1 = pC->pGates + j;
    if(pG1->nBits != 2) continue;
    for(IntVectItr it = pC->cnotOverlaps[j].begin(); it != pC->cnotOverlaps[j].end(); it++){
      int i = *it;
      pG2 = pC->pGates + i;
      cnotOverlaps2way[j].push_back(i);
      cnotOverlaps2way[i].push_back(j);
    }
  }
  for(j=0; j<pC->nGates; j++){
    pG1 = pC->pGates + j;
    if(pG1->nBits != 2) continue;
    cnotConflicts[j] = IntVect();
    for(IntVectItr it = cnotOverlaps2way[j].begin(); it != cnotOverlaps2way[j].end(); it++){
      int i = *it;
      pG2 = pC->pGates + i;
      int is_overlap;
      is_overlap = !(RECT_NOT_OVERLAP(ct_lx[j], ct_ly[j], ct_rx[j], ct_ry[j], ct_lx[i], ct_ly[i], ct_rx[i], ct_ry[i]));
      cnotConflicts[j].push_back(is_overlap);
    }
  }
}

void EJFSchedule::compute_rr_vars(){
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  SMTOutput *pMap = pInitMap;
  int j;
  Gate *pG1;

  for(j=0; j<pC->nGates; j++){
    pG1 = pC->pGates + j;
    if(pG1->nBits != 2) continue;
      int g1c, g1t;
      int lx_g1, ly_g1;
      int rx_g1, ry_g1;
      g1c = (pG1->pQubitList[0])->id;
      g1t = (pG1->pQubitList[1])->id;
      lx_g1 = MIN(pMap->qx[g1c], pMap->qx[g1t]);
      ly_g1 = MIN(pMap->qy[g1c], pMap->qy[g1t]);
      rx_g1 = MAX(pMap->qx[g1c], pMap->qx[g1t]);
      ry_g1 = MAX(pMap->qy[g1c], pMap->qy[g1t]);
      ct_lx[j] = lx_g1;
      ct_ly[j] = ly_g1;
      ct_rx[j] = rx_g1;
      ct_ry[j] = ry_g1;
  }
}

int EJFSchedule::update_time_rr(int gate_id, int &time){
  assert(gParams.routingPolicy == RECTANGLE_RSRV);

  int j;
  int i = gate_id;
  for(j=0; j<cnotOverlaps2way[i].size(); j++){
    if(cnotConflicts[i][j] == 1){
      int conflict_gate = cnotOverlaps2way[i][j];
      if(start_time[conflict_gate] != -1){
        time = MAX(time, start_time[conflict_gate] + duration[conflict_gate]);
      }
    }
  }
}

int EJFSchedule::check_routing_conflicts(){
  int j;
  Circuit *pC = pCircuit;
  int i;
  for(i=0; i<pC->nGates; i++){
    if(pC->pGates[i].nBits != 2){
      continue;
    }
    for(j=0; j<cnotOverlaps2way[i].size(); j++){
      int g2 = cnotOverlaps2way[i][j];
      if(gParams.routingPolicy == RECTANGLE_RSRV){
        int is_overlap = !(RECT_NOT_OVERLAP(ct_lx[g2], ct_ly[g2], ct_rx[g2], ct_ry[g2], ct_lx[i], ct_ly[i], ct_rx[i], ct_ry[i]));
        int is_overlap_time = !(start_time[g2] >= start_time[i] + duration[i] || start_time[i] >= start_time[g2] + duration[g2]);
        if(is_overlap && is_overlap_time) cout << i << " " << g2 << "overlap\n" << endl;
      }
      if(gParams.routingPolicy == ONE_BEND_RSRV){
        int cj_cj = RECT_NOT_OVERLAP(cj_lx[i], cj_ly[i], cj_rx[i], cj_ry[i], cj_lx[g2], cj_ly[g2], cj_rx[g2], cj_ry[g2]);
        int cj_jt = RECT_NOT_OVERLAP(cj_lx[i], cj_ly[i], cj_rx[i], cj_ry[i], jt_lx[g2], jt_ly[g2], jt_rx[g2], jt_ry[g2]);
        int jt_cj = RECT_NOT_OVERLAP(jt_lx[i], jt_ly[i], jt_rx[i], jt_ry[i], cj_lx[g2], cj_ly[g2], cj_rx[g2], cj_ry[g2]);
        int jt_jt = RECT_NOT_OVERLAP(jt_lx[i], jt_ly[i], jt_rx[i], jt_ry[i], jt_lx[g2], jt_ly[g2], jt_rx[g2], jt_ry[g2]);
        int is_not_overlap = cj_cj && cj_jt && jt_cj && jt_jt;
        int is_overlap = !is_not_overlap;
        int is_overlap_time = !(start_time[g2] >= start_time[i] + duration[i] || start_time[i] >= start_time[g2] + duration[g2]);
        if(is_overlap && is_overlap_time) cout << i << " " << g2 << "overlap\n" << endl;
      }
    }
  }
}


int EJFSchedule::one_bend_check_time(int gate_id, int time){
  int j;
  int i = gate_id;
  for(j=0; j<cnotOverlaps2way[i].size(); j++){
    if(cnotConflicts[i][j] == 1){ //if rectangles overlap
      int c = cnotOverlaps2way[i][j];
      if(start_time[c] != -1){
        //check 1bp conflict for this junction, if conflict, take max over time
        int cj_cj = RECT_NOT_OVERLAP(cj_lx[i], cj_ly[i], cj_rx[i], cj_ry[i], cj_lx[c], cj_ly[c], cj_rx[c], cj_ry[c]);
        int cj_jt = RECT_NOT_OVERLAP(cj_lx[i], cj_ly[i], cj_rx[i], cj_ry[i], jt_lx[c], jt_ly[c], jt_rx[c], jt_ry[c]);
        int jt_cj = RECT_NOT_OVERLAP(jt_lx[i], jt_ly[i], jt_rx[i], jt_ry[i], cj_lx[c], cj_ly[c], cj_rx[c], cj_ry[c]);
        int jt_jt = RECT_NOT_OVERLAP(jt_lx[i], jt_ly[i], jt_rx[i], jt_ry[i], jt_lx[c], jt_ly[c], jt_rx[c], jt_ry[c]);
        int is_not_overlap = cj_cj && cj_jt && jt_cj && jt_jt;
        int is_conflict = !is_not_overlap;
        if(is_conflict){
          time = MAX(time, start_time[c] + duration[c]);
        }
      }
    }
  }
  return time;
}

int EJFSchedule::set_one_bend_junction(int gate_id, int jx, int jy){
  Circuit *pC = pCircuit;
  SMTOutput *pMap = pInitMap;

  Gate *pG1 = pCircuit->pGates + gate_id;
  int gc, gt;
  gc = (pG1->pQubitList[0])->id;
  gt = (pG1->pQubitList[1])->id;

  cj_lx[gate_id] = MIN(pMap->qx[gc], jx);
  cj_ly[gate_id] = MIN(pMap->qy[gc], jy);
  cj_rx[gate_id] = MAX(pMap->qx[gc], jx);
  cj_ry[gate_id] = MAX(pMap->qy[gc], jy);

  jt_lx[gate_id] = MIN(pMap->qx[gt], jx);
  jt_ly[gate_id] = MIN(pMap->qy[gt], jy);
  jt_rx[gate_id] = MAX(pMap->qx[gt], jx);
  jt_ry[gate_id] = MAX(pMap->qy[gt], jy);
}

int EJFSchedule::update_time_1bp(int gate_id, int &time){
  assert(gParams.routingPolicy == ONE_BEND_RSRV);
  Circuit *pC = pCircuit;
  SMTOutput *pMap = pInitMap;

  Gate *pG1 = pCircuit->pGates + gate_id;
  int gc, gt;
  gc = (pG1->pQubitList[0])->id;
  gt = (pG1->pQubitList[1])->id;

  int j1x = pMap->qx[gc];
  int j1y = pMap->qy[gt];
  
  set_one_bend_junction(gate_id, j1x, j1y);
  int ret_time1 = one_bend_check_time(gate_id, time);

  int j2x = pMap->qx[gt];
  int j2y = pMap->qy[gc];

  set_one_bend_junction(gate_id, j2x, j2y);
  int ret_time2 = one_bend_check_time(gate_id, time);
  
  if(ret_time1 <= ret_time2){
    //cout << "Select 1" << endl;
    time = ret_time1;
    set_one_bend_junction(gate_id, j1x, j1y);
  }else{
    //cout << "Select 2" << endl;
    time = ret_time2;
    set_one_bend_junction(gate_id, j2x, j2y);
  }
}

void EJFSchedule::schedule_next_gate(){
  //determine unscheduled ready gates (dependencies met)
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  int i,j;
  Gate *pGate;
  Gate *pInGate;
  int id_cur_gate;
  int id_in_gate;

  IntVect readyList;
  for(i=0; i<pC->nGates; i++){
    pGate = pC->pGates + i;
    id_cur_gate = pGate->id;
    if(start_time[id_cur_gate] == -1){
      int ready = TRUE;
      for(j=0; j<pGate->nIn; j++){
        pInGate = pGate->pInGates[j];
        id_in_gate = pInGate->id; 
        if(start_time[id_in_gate] == -1){
          ready = FALSE;
        }
      }
      if(ready) readyList.push_back(i);
    }
  }

  //determine earliest start time for each unscheduled gate
  IntVect startTime;
  for(IntVectItr itr = readyList.begin(); itr != readyList.end(); itr++){
    i = *itr;
    //for a gate, start time is max of its dependent's end times, end time of all conflicted gates 
    int time = 0;
    pGate = pC->pGates + i;
    for(j=0; j<pGate->nIn; j++){
      pInGate = pGate->pInGates[j];
      id_in_gate = pInGate->id;
      time = MAX(time, start_time[id_in_gate] + duration[id_in_gate]);
    }
    if(pGate->nBits == 2){
      if(gParams.routingPolicy == RECTANGLE_RSRV){
        update_time_rr(i, time);
      }
      if(gParams.routingPolicy == ONE_BEND_RSRV){
        update_time_1bp(i, time);
      } 
    }
    //cout << "gate " << i << " time " << time << endl;
    startTime.push_back(time+1);
  }


  int min_time=BILLION;
  int min_id;
  for(i=0; i<readyList.size(); i++){
    if(startTime[i] < min_time){
      min_time = startTime[i];
      min_id = i;
    }
  }
  int selected_gate = readyList[min_id];
  //cout << "Selecting " << readyList[min_id] << " " << min_time << endl;
  start_time[selected_gate] = min_time;
}

void EJFSchedule::compute_schedule(){
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  int i;

  _compute_durations();
  compute_rr_vars();
  _compute_cnot_conflicts();

  for(i=0; i<MAX_GATE_COUNT; i++){
    start_time[i] = -1;
  }
 
  for(i=0; i<pC->nGates; i++) 
    schedule_next_gate();

  int max_time=0;
  for(i=0; i<pC->nGates; i++){
    max_time = MAX(max_time, start_time[i] + duration[i]);
  }

  cout << "Max time:" << max_time << endl;
  for(i=0; i<pC->nGates; i++){
    pInitMap->g.push_back(start_time[i]);
    pInitMap->d.push_back(duration[i]);
  }

  SMTOutput *pOut = pInitMap;
  check_routing_conflicts();
  /*
  while(1){
    pOut->_fix_earliest_conflict();
    int num_conflicts = pOut->_num_conflicts();
    cout << "Num conflicts:" << num_conflicts << " Last gate:" << pOut->_last_gate_time() << "\n";
    if(num_conflicts == 0) break;
  }
  */
}
