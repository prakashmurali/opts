#include "main.h"

void SMTSchedule::_set_qubit_mapping(SolverType &opt, expr_vector &qx, expr_vector &qy, SMTOutput *pOut){
  int i;
  Circuit *pC = pCircuit;
  for(i=0; i<pC->nQubits; i++)
    opt.add(qx[i] == pOut->qx[i]); 
  for(i=0; i<pC->nQubits; i++)
    opt.add(qy[i] == pOut->qy[i]); 
}

void SMTSchedule::create_sr_instance(context &c, SolverType &opt, SMTOutput *pOut){
  assert(pCircuit->nQubits <= pMachine->nQubits);
  cout << "Creating SR instance...\n";
  //qubit mapping vars: dummy, will be hardwired
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

  _set_qubit_mapping(opt, qx, qy, pOut);
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
  
  if(gParams.routingPolicy == RECTANGLE_RSRV) 
    _add_rectangle_reservation_constraints(opt, c, qx, qy, g, d);
  else if(gParams.routingPolicy == ONE_BEND_RSRV)
    _add_1bend_reservation_constraints(opt, c, qx, qy, g, d);
  else {assert (0);}

  cout << "Created SR instance\n";  
}


