#include "main.h"

//up id
int Machine::get_id2uid(int id){
  int x = _get_id2i(this, id);
  int y = _get_id2j(this, id);
  int xp = x-1;
  int yp = y;
  if(xp >= 0) return (_get_ij2id(this, xp, yp));
  else return(INVALID);
}

//down id
int Machine::get_id2did(int id){
  int x = _get_id2i(this, id);
  int y = _get_id2j(this, id);
  int xp = x+1;
  int yp = y;
  if(xp < nRows) return (_get_ij2id(this, xp, yp));
  else return(INVALID);
}

//left id
int Machine::get_id2lid(int id){
  int x = _get_id2i(this, id);
  int y = _get_id2j(this, id);
  int xp = x;
  int yp = y-1;
  if(yp >= 0) return (_get_ij2id(this, xp, yp));
  else return(INVALID);
}

//right id
int Machine::get_id2rid(int id){
  int x = _get_id2i(this, id);
  int y = _get_id2j(this, id);
  int xp = x;
  int yp = y+1;
  if(yp < nCols) return (_get_ij2id(this, xp, yp));
  else return(INVALID);
}

void Machine::_add_neighbor_if_valid(HwQubit *pQ, int id){
  if(id != INVALID) pQ->pEdgeList[pQ->nEdges++] = pQubitList + id;
}

void Machine::setup_grid_transmons(int m, int n){
  nRows = m;
  nCols = n;
  nQubits = m*n;
  pQubitList = new HwQubit[nQubits];
  int i;
  HwQubit *pQ;
  cout << "nrows: " << nRows << " nCols: " << nCols << "\n";
  for(i=0; i<nQubits; i++){
    pQ = pQubitList + i;
    
    // position of this qubit
    pQ->id = i;
    pQ->pos_x = _get_id2i(this, i);
    pQ->pos_y = _get_id2j(this, i);
    
    // add edges for this qubit
    pQ->nEdges = 0;
    _add_neighbor_if_valid(pQ, get_id2uid(i));
    _add_neighbor_if_valid(pQ, get_id2did(i));
    _add_neighbor_if_valid(pQ, get_id2lid(i));
    _add_neighbor_if_valid(pQ, get_id2rid(i));
  }
}

void Machine::setup_gate_times(){
  pProp = new MachineProp;
  pProp->gateTime["CNOT"] = gParams.time_CNOT;
  pProp->gateTime["X"] = gParams.time_X;
  pProp->gateTime["Y"] = gParams.time_Y;
  pProp->gateTime["H"] = gParams.time_H;
  pProp->gateTime["MeasZ"] = gParams.time_MeasZ;
  
  //derived time
  pProp->gateTime["SWAP"] = (gParams.time_CNOT * 3) + (gParams.time_H * 2);

  /* TODO below times are actually 0, they are software gates */
  pProp->gateTime["Z"] = 1;
  pProp->gateTime["S"] = 1;
  pProp->gateTime["Sdag"] = 1;
  pProp->gateTime["T"] = 1;
  pProp->gateTime["Tdag"] = 1;
  pProp->gateTime["PrepZ"] = 1;
  pProp->gateTime["MeasZ"] = 1;
}
