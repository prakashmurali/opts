#include "main.h"

/* Utils */

void SMTSchedule::_print_qubit_mapping(model &m, expr_vector &qx, expr_vector &qy){
  int i;
  Circuit *pC = pCircuit;
  cout << "Qubit Mapping\n";
  for(i=0; i<pC->nQubits; i++){
    cout << "Q" << i << " ";
    cout << m.eval(qx[i]) << ", ";
    cout << m.eval(qy[i]) << "\n";
  }
}

void SMTSchedule::_print_gate_times(model &m, expr_vector &g, expr_vector &d){
  int j;
  Circuit *pC = pCircuit;
  cout << "Gate time slots\n";
  for(j=0; j<pC->nGates; j++){
    cout << "G" << j << " s:" << m.eval(g[j]) << " d:" << m.eval(d[j]) << "\n";
  }
}

void SMTSchedule::_copy_qubit_mapping(SMTOutput *pOut, model &m, expr_vector &qx, expr_vector &qy){
  int i;
  Circuit *pC = pCircuit;
  for(i=0; i<pC->nQubits; i++){
    pOut->qx.push_back(m.eval(qx[i]).get_numeral_int());
    pOut->qy.push_back(m.eval(qy[i]).get_numeral_int());
  }
}

void SMTSchedule::_copy_gate_times(SMTOutput *pOut, model &m, expr_vector &g, expr_vector &d){
  int j;
  Circuit *pC = pCircuit;
  for(j=0; j<pC->nGates; j++){
    pOut->g.push_back(m.eval(g[j]).get_numeral_int());
    pOut->d.push_back(m.eval(d[j]).get_numeral_int());
  }
}

void SMTSchedule::_copy_cnot_route(SMTOutput *pOut, model &m, context &c){
  int j, jIdx;
  Circuit *pC = pCircuit;
  Gate *pG;
  for(j=0, jIdx=0; j<pC->nGates; j++){
    pG = pC->pGates + j;
    if(pG->nBits != 2){
      pOut->cnotIdx.push_back(INVALID);
      continue;
    }
    stringstream jx_name, jy_name;
    jx_name << "cnot1b_jx_" << j;
    jy_name << "cnot1b_jy_" << j;
    pOut->jx.push_back(m.eval(c.int_const(jx_name.str().c_str())).get_numeral_int());
    pOut->jy.push_back(m.eval(c.int_const(jy_name.str().c_str())).get_numeral_int());
    //cout << m.eval(c.int_const(jx_name.str().c_str())).get_numeral_int() << " " << m.eval(c.int_const(jy_name.str().c_str())).get_numeral_int() << "\n";
    pOut->cnotIdx.push_back(jIdx);
    jIdx++;
  }
}

void init_stats(){
  gStats.cntMapping = 0;
  gStats.cntDepend = 0;
  gStats.cntRouting = 0;
  gStats.z3_arith_conflicts = 0;
  gStats.z3_conflicts = 0;
  gStats.z3_arith_add_rows = 0;
  gStats.z3_mk_clause = 0;
  gStats.z3_added_eqs = 0;
  gStats.z3_binary_propagations = 0;
  gStats.z3_restarts = 0;
  gStats.z3_decisions = 0;
}

void accumulate_stats(stats &S){
  int i;
  for(i=0; i<S.size()-1; i++){
    string k = S.key(i);
    int val;
    if(S.is_uint(i))
      val = S.uint_value(i);
    else
      continue;
    if(k == "decisions") gStats.z3_decisions += val;
    if(k == "arith conflicts") gStats.z3_arith_conflicts += val;
    if(k == "conflicts") gStats.z3_conflicts += val;
    if(k == "arith add rows") gStats.z3_arith_add_rows += val;
    if(k == "mk clause") gStats.z3_mk_clause += val;
    if(k == "added eqs") gStats.z3_added_eqs += val;
    if(k == "binary propagations") gStats.z3_binary_propagations += val;
    if(k == "restarts") gStats.z3_restarts += val;

  }  
}

void print_stats(){
  cout << "Constraints:\n";
  cout << "Mapping:" << gStats.cntMapping << endl;
  cout << "Dependency:" << gStats.cntDepend << endl;
  cout << "Routing:" << gStats.cntRouting << endl;
  cout << "Stats:\n";
  cout << "arith_conflicts " << gStats.z3_arith_conflicts << endl;
  cout << "arith_add_rows " << gStats.z3_arith_add_rows << endl;
  cout << "conflicts " <<  gStats.z3_conflicts << endl;
  cout << "decisions " << gStats.z3_decisions << endl;
  cout << "mk_clause " <<  gStats.z3_mk_clause << endl;
  cout << "added_eqs " << gStats.z3_added_eqs << endl;
  cout << "binary_propagations " << gStats.z3_binary_propagations << endl;
  cout << "restarts " << gStats.z3_restarts << endl;
}

void print_gd(int id, int g, int d, string gate_name){
  cout << gate_name << " " << id << " (" << g << "," << d << ")\n";
}

/* Circuit print methods */

std::ostream& operator<< (std::ostream &out, const Qubit &q){
  out << q.id << endl;
  return out;
}

std::ostream& operator<< (std::ostream &out, const Gate &g){
  out << "Gate:" << g.id << " " << g.type << " ";
  out << "Qubits:";
  int i;
  for(i=0; i<g.nBits; i++){
    out << *(g.pQubitList[i]) << " ";
  }
  out << "In gates:";
  for(i=0; i<g.nIn; i++) out << (g.pInGates[i])->id << " ";
  out << "\n"; 
  return out;
}

std::ostream& operator<< (std::ostream &out, const Circuit &c){
  int i;
  out << "Circuit qubits:" << c.nQubits << " gates:" << c.nGates << "\n";
  for(i=0; i<c.nGates; i++){
    out << c.pGates[i];
  }
}

struct timespec ts_time_now;
int MyGetTime(){
  clock_gettime(CLOCK_MONOTONIC, &ts_time_now);
  return (ts_time_now.tv_sec);
}


