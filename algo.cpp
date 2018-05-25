#include "main.h"

MapAlgo::MapAlgo(Circuit *pC, Machine *pM){
  pCircuit = pC;
  pMachine = pM;
}

void MapAlgo::create_program_graph(Circuit *pC){
  int j;
  Gate *pG;
  for(j=0; j<pC->nGates; j++){
    pG = pC->pGates + j;
    if(pG->nBits == 2){
      Qubit *pQ1 = pG->pQubitList[0]; 
      Qubit *pQ2 = pG->pQubitList[1];
      int Q1id = pQ1->id;
      int Q2id = pQ2->id;
      //graph has an edge between Q1id and Q2id
      VertexList.insert(Q1id);
      VertexList.insert(Q2id);
      EdgeList.push_back(make_pair(Q1id, Q2id));
    }
    else{
      Qubit *pQ1 = pG->pQubitList[0];
      int Q1id = pQ1->id;
      VertexList.insert(Q1id);
    }
  }
#if 0  
  vector<ProgEdge>::iterator itr;
  for(itr = EdgeList.begin(); itr != EdgeList.end(); itr++){
    ProgEdge x = *itr;
    cout << x.first << " " << x.second << "\n";
  }

  for(IntSetItr itrs = VertexList.begin(); itrs != VertexList.end(); itrs++){
    cout << *itrs << "\n";
  }
#endif
}

void MapAlgo::program_id_mapping(){
  for(IntSetItr itrs = VertexList.begin(); itrs != VertexList.end(); itrs++){
    int qid = *itrs;
    int i = _get_id2i(pMachine, qid);
    int j = _get_id2j(pMachine, qid);
    qx.push_back(i);
    qy.push_back(j);
  }
}

void MapAlgo::copy_mapping(SMTOutput *pOut){
  for(int i=0; i < qx.size(); i++){
    pOut->qx.push_back(qx[i]);
    pOut->qy.push_back(qy[i]); 
  }
}
