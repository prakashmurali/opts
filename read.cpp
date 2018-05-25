#include "main.h"

void Circuit::input_from_file(string fname){
  ifstream fileIn;
  cout << "Reading " << fname << "\n";
  fileIn.open(fname.c_str());
  fileIn >> nQubits >> nGates;
  pQubits = new Qubit[nQubits];
  pGates = new Gate[nGates];
  int i,j,tmp;
  Gate *pG;

  for(i=0; i<nQubits; i++){
    pQubits[i].id = i;
  }
  for(i=0; i<nGates; i++){
    pG = &(pGates[i]);
    fileIn >> pG->id >> pG->type >> pG->nBits;
    assert(pG->nBits <= 2);
    for(j=0; j<pG->nBits; j++){
      fileIn >> tmp;
      pG->pQubitList[j] = &pQubits[tmp];
    }
    fileIn >> pG->nIn;
    assert(pG->nIn <= 4);
    for(j=0; j<pG->nIn; j++){
      fileIn >> tmp;
      pG->pInGates[j] = &pGates[tmp];
    }

  }
  fileIn.close();
}

void Circuit::input_cnot_overlap_info(string fname){
  ifstream fileIn;
  cout << "Reading " << fname << "\n";
  fileIn.open(fname.c_str());
  int nCNOTS;
  fileIn >> nCNOTS;
  int i,j;
  int id, len, tmp;
  for(i=0; i<nCNOTS; i++){
    fileIn >> id >> len;
    cnotOverlaps[id] = IntVect();
    for(j=0; j<len; j++){
      fileIn >> tmp;
      cnotOverlaps[id].push_back(tmp);
    }
  }
}

void Circuit::input_gate_descendants(string fname){
  ifstream fileIn;
  cout << "Reading " << fname << "\n";
  fileIn.open(fname.c_str());
  int nG;
  fileIn >> nG;
  assert(nG == nGates);
  int i,j;
  int id, len, tmp;
  for(i=0; i<nG; i++){
    fileIn >> id >> len;
    gateDepends[id] = IntVect();
    for(j=0; j<len; j++){
      fileIn >> tmp;
      gateDepends[id].push_back(tmp);
    }
  }
}

void Circuit::input_program_graph(string fname){
  ifstream fileIn;
  cout << "Reading " << fname << "\n";
  fileIn.open(fname.c_str());
  fileIn >> nEdges;
  fileIn >> nNodes;
  int i;
  for(i=0; i<nEdges; i++){
    char type;
    int edge_id;
    int u;
    int v;
    int w;
    fileIn >> type;
    assert(type == 'E');
    fileIn >> edge_id >> u >> v >> w;
    graphEdges[edge_id] = IntVect();
    graphEdges[edge_id].push_back(u);
    graphEdges[edge_id].push_back(v);
    graphEdges[edge_id].push_back(w);
  }
  for(i=0; i<nNodes; i++){
    char type;
    int cnt;
    int nid;
    fileIn >> type;
    assert(type == 'N');
    fileIn >> nid;
    fileIn >> cnt;
    nodeEdgeIds[nid] = IntVect();
    int eid;
    for(int j=0; j<cnt; j++){
      fileIn >> eid;
      nodeEdgeIds[nid].push_back(eid);
    }
  }
}

void Circuit::input_greedy_mapping(string fname){
  ifstream fileIn;
  cout << "Reading " << fname << "\n";
  fileIn.open(fname.c_str());
  for(int i=0; i<nQubits; i++){
    int q;
    int x;
    int y;
    fileIn >> q >> x >> y;
    qx[q] = x;
    qy[q] = y;
  } 
}

void Machine::input_coherence_data(string fname){
  ifstream fileIn;
  cout << "Reading " << fname << "\n";
  fileIn.open(fname.c_str());
  assert(nQubits == 16); //Only IBMqx5 supported now
  
  int qx, qy, T2;
  int i;
  for(i=0; i<16; i++){
    fileIn >> qx >> qy >> T2;
    pProp->cohrTime[qx][qy] = T2; 
  }
}

void Machine::input_rr_cnot_data(string fname){
  ifstream fileIn;
  cout << "Reading " << fname << "\n";
  fileIn.open(fname.c_str());
  assert(nQubits == 16); //Only IBMqx5 supported now
  
  int Cqx, Cqy, Tqx, Tqy, time;
  int i;
  for(i=0; i<256; i++){
    fileIn >> Cqx >> Cqy >> Tqx >> Tqy >> time;
    pProp->cnotRRTime[Cqx][Cqy][Tqx][Tqy] = time;
  }
}

void Machine::input_one_bend_cnot_data(string fname){
  ifstream fileIn;
  cout << "Reading " << fname << "\n";
  fileIn.open(fname.c_str());
  assert(nQubits == 16); //Only IBMqx5 supported now
  
  int Cqx, Cqy, Tqx, Tqy, Jqx, Jqy, time;
  int i;
  for(i=0; i<256; i++){
    fileIn >> Cqx >> Cqy >> Tqx >> Tqy >> Jqx >> Jqy >> time;
    pProp->cnotOneBendTime[Cqx][Cqy][Tqx][Tqy][0][0] = Jqx;
    pProp->cnotOneBendTime[Cqx][Cqy][Tqx][Tqy][0][1] = Jqy;
    pProp->cnotOneBendTime[Cqx][Cqy][Tqx][Tqy][0][2] = time;

    fileIn >> Cqx >> Cqy >> Tqx >> Tqy >> Jqx >> Jqy >> time;
    pProp->cnotOneBendTime[Cqx][Cqy][Tqx][Tqy][1][0] = Jqx;
    pProp->cnotOneBendTime[Cqx][Cqy][Tqx][Tqy][1][1] = Jqy;
    pProp->cnotOneBendTime[Cqx][Cqy][Tqx][Tqy][1][2] = time;
  }
}

