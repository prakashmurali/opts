#include "main.h"

SMTOutput::SMTOutput(Circuit *pC, Machine *pM){
  pCircuit = pC;
  pMachine = pM;
}

void SMTOutput::load_qubit_mapping(int *pQx, int *pQy){
  for(int i=0; i<pCircuit->nQubits; i++){
    qx.push_back(pQx[i]);
    qy.push_back(pQy[i]);
  }
}

void SMTOutput::_check_mapping_bounds(){
  int R = gParams.machineRows;
  int C = gParams.machineCols;
  for(IntVectItr itr=qx.begin(); itr != qx.end(); itr++){
    assert(*itr >= 0);
    assert(*itr < R);
  }
  for(IntVectItr itr=qy.begin(); itr != qy.end(); itr++){
    assert(*itr >= 0);
    assert(*itr < C);
  }
  cout << "Mapping bounds check passed\n";
}

void SMTOutput::_check_unique_mapping(){
  int i,j;
  for(i=0; i<pCircuit->nQubits; i++){
    for(j=i+1; j<pCircuit->nQubits; j++){
      if(!(qx[i] != qx[j] || qy[i] != qy[j])){
        cout << i << " " << j << " " << qx[i] << " " << qx[j] << " " << qy[i] << " " << qy[j] <<  endl;
      }
      assert(qx[i] != qx[j] || qy[i] != qy[j]);
    }
  }
  cout << "Unique bounds check passed\n";
}

void SMTOutput::_check_gate_time_bounds(){
  int maxT = gParams.maxTimeSlot;
  for(int i=0; i<pCircuit->nGates; i++){
    assert((g[i] + d[i]) < maxT);
  } 
  cout << "Gate time check passed\n";
}

void SMTOutput::_check_dependency_constraint(){
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
      if(g[id_cur_gate] < g[id_in_gate] + d[id_in_gate]){
        cout << "Violation\n"; 
        print_gd(id_cur_gate, g[id_cur_gate], d[id_cur_gate], "Current");
        print_gd(id_in_gate, g[id_in_gate], d[id_in_gate], "In gate");
      }
      assert(g[id_cur_gate] >= g[id_in_gate] + d[id_in_gate]);
    }
  }
  cout << "Gate dependency check passed\n";
}

void SMTOutput::_check_gate_durations(){
  int i;
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  MachineProp *pProp = pMachine->pProp;
  Gate *pG;
  for(i=0; i<pC->nGates; i++){
    pG = pC->pGates + i;
    if(pG->nBits == 1){
      assert(d[i] == pProp->gateTime[pG->type]-1);
    }else if(pG->nBits == 2){
      //compute distance between control and target and find duration
      Qubit *pQ1 = pG->pQubitList[0]; 
      Qubit *pQ2 = pG->pQubitList[1];
      int id1 = pQ1->id;
      int id2 = pQ2->id;
      int dist = abs(qx[id1] - qx[id2]) + abs(qy[id1] - qy[id2]);
      int duration = 2*((dist-1)*pProp->gateTime["SWAP"]) + pProp->gateTime[pG->type] - 1;
      assert(duration == d[i]);
    }
  }
  cout << "Duration check passed\n";
}

class GateEvent{
  public:
    int timestamp;
    int duration;
    int etype;
    Gate *pGate;
    GateEvent(int ts, int d, int et, Gate *pG) : timestamp(ts), duration(d), etype(et), pGate(pG) {}
};

class GateEventCmp{
  public:
    inline bool operator() (const GateEvent &e1, const GateEvent &e2){
      if(e1.timestamp < e2.timestamp) return true;
      if(e2.timestamp < e1.timestamp) return false;

      //timestamps are equal
      return (e1.etype < e2.etype);
    }
};

void SMTOutput::_update_block_map_single_qubit(vector <vector <int> > &blkd, int qX, int qY, int t, int duration, int etype){
  int qid = _get_ij2id(pMachine, qX, qY);
  if(etype == 0){ //start 
    for(int i=t; i<=t+duration; i++){
      assert(blkd[qid][i] == 0);
      blkd[qid][i] = 1;
    }
  }
}

IntSet* SMTOutput::_find_qubits_to_block(int q1X, int q1Y, int q2X, int q2Y){
  int lX, lY;
  int rX, rY;
  lX = MIN(q1X, q2X);
  lY = MIN(q1Y, q2Y);
  rX = MAX(q1X, q2X);
  rY = MAX(q1Y, q2Y);
  int D = gParams.couplingThresh;
  IntSet *pSet = new IntSet;
  for(int q=0; q<pMachine->nQubits; q++){
    int pX = _get_id2i(pMachine, q);
    int pY = _get_id2j(pMachine, q);
    int is_inside = POINT_IN_RECT(lX-D, lY-D, rX+D, rY+D, pX, pY);
    if(is_inside) pSet->insert(q);
  }
  return(pSet);
}

void SMTOutput::_block_qubits_for_duration(vector <vector <int> > &blkd, IntSet *pSet, int t, int duration){
  IntSetItr itr = pSet->begin();
  int q;
  for(; itr!=pSet->end(); itr++){
    q = *itr;
    for(int i=t; i<=t+duration; i++){
      assert(blkd[q][i] == 0);
      blkd[q][i] = 1;
    }
  }
}

void SMTOutput::_check_routing_constraints(){
  int maxT = gParams.maxTimeSlot; 
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  //machine qubits x T
  vector< vector<int> > blkd(pM->nQubits, vector<int>(maxT));
  int q,t;
  for(q=0; q<pM->nQubits; q++){
    for(t=0; t<maxT; t++){
      blkd[q][t] = 0;
    }
  }

  //find the order of events (t, gate) of gate starts and finish times
  std::vector<GateEvent> events;
  Gate *pGate;
  int id_cur_gate;
  for(int i=0; i<pC->nGates; i++){
    pGate = pC->pGates + i;
    id_cur_gate = pGate->id;
    events.push_back(GateEvent(g[id_cur_gate], d[id_cur_gate], 0, pGate)); 
    events.push_back(GateEvent(g[id_cur_gate]+d[id_cur_gate], d[id_cur_gate], 1, pGate));
  }
  std::sort(events.begin(), events.end(), GateEventCmp());

  //run through the gate start/end events
  GateEvent *pGE;
  Qubit *pQ1, *pQ2; 
  Gate *pG;
  int duration;
  for(std::vector<GateEvent>::iterator itr = events.begin(); itr != events.end(); itr++){
    pGE = &(*itr);
    pG = pGE->pGate;
    t = pGE->timestamp;
    duration = pGE->duration;
    //for each gate, find its machine qubits, use machine qubit locations and gate time to block 
    if(pG->nBits == 1){
      pQ1 = pG->pQubitList[0]; 
      int qid = pQ1->id;
      _update_block_map_single_qubit(blkd, qx[qid], qy[qid], t, duration, pGE->etype);
    }else if(pG->nBits == 2){
      int cid, tid;
      pQ1 = pG->pQubitList[0];
      pQ2 = pG->pQubitList[1];
      cid = pQ1->id;
      tid = pQ2->id;
      if(pGE->etype == 0){
        if(gParams.routingPolicy == RECTANGLE_RSRV){
          IntSet *pBlock = _find_qubits_to_block(qx[cid], qy[cid], qx[tid], qy[tid]);
          _block_qubits_for_duration(blkd, pBlock, t, duration);
          delete pBlock;
        }else if(gParams.routingPolicy == ONE_BEND_RSRV){
          int jX = jx[cnotIdx[pG->id]];
          int jY = jy[cnotIdx[pG->id]];
          IntSet *pBlock1 = _find_qubits_to_block(qx[cid], qy[cid], jX, jY);
          IntSet *pBlock2 = _find_qubits_to_block(qx[tid], qy[tid], jX, jY);
          IntSet *pSet = new IntSet;
          for(IntSetItr itr = pBlock1->begin(); itr != pBlock1->end(); itr++) pSet->insert(*itr);
          for(IntSetItr itr = pBlock2->begin(); itr != pBlock2->end(); itr++) pSet->insert(*itr);
          delete pBlock1;
          delete pBlock2;
          _block_qubits_for_duration(blkd, pSet, t, duration);
          delete pSet;
        }
      }
    }
  }
  cout << "Routing constraints check passed\n";
}

void SMTOutput::check_sanity(){
  _check_mapping_bounds();
  //_check_unique_mapping();
  //_check_gate_time_bounds();
  _check_gate_durations();
  _check_dependency_constraint();
  //_check_routing_constraints();
}

void SMTOutput::print_to_file(string fname){
  //File format: mappings for each program qubit
  //Gate id, gate start time, gate duration
  //CNOT junctions for 1-bend paths
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;

  ofstream ofile;
  ofile.open(fname.c_str());
  int i;
  int qid;
  for(i=0; i<pC->nQubits; i++){
    qid = pCircuit->pQubits[i].id;
    ofile << "Q " << qid << " "  << qx[qid] << " " << qy[qid] << "\n"; 
  }

  int gid;
  for(i=0; i<pC->nGates; i++){
    gid = pCircuit->pGates[i].id;
    ofile << "G " << gid << " " << g[gid] << " " << d[gid] << "\n";
  }

  if(gParams.routingPolicy == ONE_BEND_RSRV){
    for(i=0; i<pC->nGates; i++){
      gid = pCircuit->pGates[i].id;
      if(cnotIdx[gid] != INVALID){
        ofile << "R " << i << " " << jx[cnotIdx[i]] << " " << jy[cnotIdx[gid]] << "\n";
      }
    }
  }
}
