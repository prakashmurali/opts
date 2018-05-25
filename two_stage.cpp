#include "main.h"

void SMTSchedule::_add_edge_durations(context &c, SolverType &opt, expr_vector &qx, expr_vector &qy, expr_vector &d){
  int j;
  Circuit *pC = pCircuit;
  Machine *pM = pMachine;
  MachineProp *pProp = pM->pProp;

  for(j=0; j<pC->nEdges; j++){
    int u, v, w;
    u = pC->graphEdges[j][0];
    v = pC->graphEdges[j][1];
    w = pC->graphEdges[j][2];
    expr distance = DISTANCE(qx[u], qy[u], qx[v], qy[v]);
    opt.add(d[j] == w*( 2*((distance-1)*pProp->gateTime["SWAP"]) + pProp->gateTime["CNOT"]-1) ); //2*swap time + cnot time
    //opt.add(d[j] == distance*2*w - w);
  }

  for(j=0; j<pC->nNodes; j++){
    expr_vector myvars(c);
    int flag=0;
    for(IntVectItr itr = pC->nodeEdgeIds[j].begin();
        itr != pC->nodeEdgeIds[j].end();
        itr++){
      myvars.push_back(d[*itr]);
      flag = 1;
    }
    if(flag){
      //opt.add(sum(myvars) < gParams.CNOT_Count_Max);
      opt.add(sum(myvars) < gParams.maxTimeSlot);
    }else{
      cout << "node id" << j << "no d" << endl;
    }

  }
}

void SMTSchedule::ts_create_mapping_instance(context &c, SolverType &opt){
  assert(pCircuit->nQubits <= pMachine->nQubits);
  cout << "TwoStage: Creating mapping instance...\n";

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

  // duration vars
  expr_vector d(c);
  int j;
  for(j=0; j<pCircuit->nEdges; j++){
    std::stringstream d_name;
    d_name << "d_" << j;
    d.push_back( c.int_const(d_name.str().c_str()) );
  }

  _add_qubit_mapping_bounds(opt, qx, qy); //space to explore
  _add_edge_durations(c, opt, qx, qy, d);

  if(opt.check() == sat){
    model m = opt.get_model();
  }else{
    cout << "Not sat!\n";
  }
}
