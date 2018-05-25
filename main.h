#ifndef __MAIN_H__
#define __MAIN_H__

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <assert.h>
#include <map>
#include <vector>
#include <time.h>
#include <cmath>
#include <algorithm>
#include <set>
#include <utility>
#include"z3++.h"


using namespace std;
using namespace z3;

#define MAX_QUBITS 1024
#define MAX_GATE_COUNT (MAX_QUBITS*20)

#define MILLION (1000*1000)
#define BILLION (1000*1000*1000)

//typedef optimize SolverType;
typedef solver SolverType;


typedef vector<int> IntVect;
typedef map<int, IntVect> Int2VectMap;
typedef vector<int>::iterator IntVectItr;
typedef set<int> IntSet;
typedef set<int>::iterator IntSetItr;
typedef pair<int, int> ProgEdge;
  
#define MIN(x, y) ((x)<(y) ? (x) : (y))
#define MAX(x, y) ((x)>(y) ? (x) : (y))
#define TRUE 1
#define FALSE 0

/* Program classes */

class Qubit{
  public:
    int id;
    string name;

    friend std::ostream& operator<< (std::ostream&, const Qubit&);
};

class Gate{
  public:
    int id;
    string type;
    int nBits;
    Qubit *pQubitList[2]; // pointers to atmost 2 qubits (assuming input has <= 2 qubit gates)
    int nIn;
    Gate *pInGates[4];    // atmost 4 incoming/outgoing dependencies
    friend std::ostream& operator<< (std::ostream&, const Gate&);
};

class Circuit{
  public:
    int nQubits;
    int nGates;
    Qubit *pQubits;
    Gate *pGates;
    Int2VectMap cnotOverlaps;
    Int2VectMap gateDepends;
   
    //greedy mapping  
    int qx[MAX_QUBITS];
    int qy[MAX_QUBITS];

    //program graph related vars
    int nEdges;
    int nNodes;
    Int2VectMap graphEdges;
    Int2VectMap nodeEdgeIds;

    void input_from_file(string fname);
    void input_cnot_overlap_info(string fname);
    void input_gate_descendants(string fname);
    void input_program_graph(string fname);
    void input_greedy_mapping(string fname);

    friend std::ostream& operator<< (std::ostream&, const Circuit&);
};

/* Hardware classes */

class HwQubit{
  public:
    int id;
    int nEdges;
    HwQubit *pEdgeList[4]; //assuming atmost 4 way conectivity for a qubit
    int pos_x;
    int pos_y;
};

#define INVALID (-1)
#define _get_ij2id(machine, i, j) (i * machine->nCols + j)
#define _get_id2i(machine, id)    (id / machine->nCols)
#define _get_id2j(machine, id)    (id % machine->nCols) 

#define MAX_ROWS 2
#define MAX_COLS 8

class MachineProp{
  public:
    map<string, int> gateTime;
    int cohrTime[MAX_ROWS][MAX_COLS];
    int cnotRRTime[MAX_ROWS][MAX_COLS][MAX_ROWS][MAX_COLS];
    int cnotOneBendTime[MAX_ROWS][MAX_COLS][MAX_ROWS][MAX_COLS][2][3]; //cx, cy, tx, ty, junctions 0/1, (jx, jy, time)
};

class Machine{
  public:
    int nQubits;
    int nRows, nCols;
    HwQubit *pQubitList;
    MachineProp *pProp; 

    void setup_grid_transmons(int m, int n);
    void setup_gate_times();
    void input_coherence_data(string fname);
    void input_rr_cnot_data(string fname);
    void input_one_bend_cnot_data(string fname);

    int get_id2uid(int id);
    int get_id2did(int id);
    int get_id2lid(int id);
    int get_id2rid(int id);
    void _add_neighbor_if_valid(HwQubit *pQ, int id);
};

/* Solver classes */

/* Routing policies */
#define RECTANGLE_RSRV 1
#define ONE_BEND_RSRV 2

#define IS_UNSAT 0
#define IS_SAT 1

#define ABSOLUTE(e) (ite((e) >= 0, (e), (-e)))
#define DISTANCE(q1x, q1y, q2x, q2y) (ABSOLUTE((q1x - q2x)) + ABSOLUTE((q1y - q2y)))
#define MIN_MAX_COND(ax, bx, minx, maxx) (ite((ax)<=(bx), (minx==(ax))&&(maxx==(bx)), (minx==(bx))&&(maxx==(ax))))
#define POINT_IN_RECT(lx, ly, rx, ry, px, py) ((((px) >= (lx)) && ((px) <= (rx))) && (((py) >= (ly)) && ((py) <= (ry))))
#define RECT_NOT_OVERLAP(l1x, l1y, r1x, r1y, l2x, l2y, r2x, r2y) ( ((l1x) > (r2x)) || ((l2x) > (r1x)) || ((l1y) > (r2y)) || ((l2y) > (r1y)) )

class SMTOutput{
  public:
    //qubit mappings
    IntVect qx;
    IntVect qy;
    //gate start and  durations
    IntVect g;
    IntVect d;
    //junction mappings for CNOT in 1-bend path reservation
    IntVect jx;
    IntVect jy;
    IntVect cnotIdx;
    int result;

    Circuit *pCircuit;
    Machine *pMachine;

    void check_sanity();
    void print_to_file(string fname);
    void load_qubit_mapping(int *, int *);
    SMTOutput(Circuit *pC, Machine *pM);
    int _check_routing_conflict(int g1, int g2, int c1, int t1, int c2, int t2);
    int _num_conflicts();
    int _fix_earliest_conflict();
    int _last_gate_time();
  private:
    void _check_mapping_bounds();
    void _check_gate_time_bounds();
    void _check_dependency_constraint();
    void _check_unique_mapping();
    void _check_gate_durations();
    void _check_routing_constraints();
    void _update_block_map_single_qubit(vector <vector <int> > &blkd, int qX, int qY, int t, int duration, int etype);
    IntSet* _find_qubits_to_block(int q1X, int q1Y, int q2X, int q2Y);
    void _block_qubits_for_duration(vector <vector <int> > &blkd, IntSet *pSet, int t, int duration);

};

class SMTSchedule{
  public:
    Circuit *pCircuit;
    Machine *pMachine;
    int maxT; // maximum allowed time slot

    SMTSchedule(Circuit *pC, Machine *pM);
    SMTOutput* compute_schedule();
    void create_optimization_instance(context &c, SolverType &opt, int routing_flag);
    SMTOutput* check_instance(context &c, SolverType &opt, int maxTimeSlot);

    //two stage, mapping 
    void ts_create_mapping_instance(context &c, SolverType &opt);
    void _add_edge_durations(context &c, SolverType &opt, expr_vector &qx, expr_vector &qy, expr_vector &d);

    //lazy routing:cegar
    void lr_create_optimization_instance(context &c, SolverType &opt);
    SMTOutput* lr_check_current_instance(context &c, SolverType &opt, int maxTimeSlot);
    int _lr_cegar_process_routing_conflicts(context &c, SolverType &opt, SMTOutput *pOut);
    SMTOutput* lr_cegar_solve(context &c, SolverType &opt, int maxTimeSlot);
    void _lr_add_conflict_clause_time(context &c, SolverType &opt, int g1, int g2);

    //lazy routing:greedy
    SMTOutput* lr_greedy_solve(context &c, SolverType &opt, int maxTimeSlot);

    //MS+SR method
    void create_sr_instance(context &c, SolverType &opt, SMTOutput *pOut);
    void _set_qubit_mapping(SolverType &opt, expr_vector &qx, expr_vector &qy, SMTOutput *pOut);

  private:

    //data unaware constraints - schedule.cpp
    void _add_qubit_mapping_bounds(SolverType&, expr_vector&, expr_vector&);
    void _add_single_qubit_gate_durations(SolverType &opt, expr_vector &d);
    void _add_gate_time_bounds(SolverType&, expr_vector&, expr_vector &);
    void _add_gate_dependency_constraints(SolverType &, expr_vector &, expr_vector &);
    void _set_dummy_gate_bound(SolverType&, expr_vector&, expr_vector &, context&, int);
    void _add_rectangle_reservation_constraints(SolverType &, context &, expr_vector &, expr_vector &, expr_vector &, expr_vector &);
    void _add_1bend_reservation_constraints(SolverType &, context &, expr_vector &, expr_vector &, expr_vector &, expr_vector &);
    void _add_cnot_durations_no_data(SolverType &opt, expr_vector &qx, expr_vector &qy, expr_vector &d);
    void _add_mirror_symmetry(SolverType &opt, expr_vector &qx, expr_vector &qy);

    //time data aware constraints - schedule.cpp
    void _add_coherence_aware_time_bounds(SolverType &opt, expr_vector &qx, expr_vector &qy, expr_vector &g, expr_vector &d);
    void _add_cnot_durations_rect_rsrv_data(SolverType &opt, expr_vector &qx, expr_vector &qy, expr_vector &d);
    void _add_cnot_durations_one_bend_data(SolverType &, context &, expr_vector &, expr_vector &, expr_vector &);
   
    //utils - utils.cpp
    void _print_qubit_mapping(model &, expr_vector &, expr_vector &);
    void _print_gate_times(model &, expr_vector &, expr_vector &);
    void _copy_qubit_mapping(SMTOutput *pOut, model &m, expr_vector &qx, expr_vector &qy);
    void _copy_gate_times(SMTOutput *pOut, model &m, expr_vector &g, expr_vector &d);
    void _copy_cnot_route(SMTOutput *pOut, model &m, context &c);
};

class EJFSchedule{
  private:
    Circuit *pCircuit;
    Machine *pMachine;
    SMTOutput *pInitMap;
    
    void _compute_durations();
    void _compute_cnot_conflicts();

  public:
    int start_time[MAX_GATE_COUNT];
    int duration[MAX_GATE_COUNT];

    Int2VectMap cnotOverlaps2way;
    Int2VectMap cnotConflicts;
    
    int ct_lx[MAX_GATE_COUNT];
    int ct_ly[MAX_GATE_COUNT];
    int ct_rx[MAX_GATE_COUNT];
    int ct_ry[MAX_GATE_COUNT];

    int cj_lx[MAX_GATE_COUNT];
    int cj_ly[MAX_GATE_COUNT];
    int cj_rx[MAX_GATE_COUNT];
    int cj_ry[MAX_GATE_COUNT];

    int jt_lx[MAX_GATE_COUNT];
    int jt_ly[MAX_GATE_COUNT];
    int jt_rx[MAX_GATE_COUNT];
    int jt_ry[MAX_GATE_COUNT];

    int jx[MAX_GATE_COUNT];
    int jy[MAX_GATE_COUNT];

    EJFSchedule(Circuit *, Machine *, SMTOutput *);
    void compute_schedule();
    void schedule_next_gate();
    void compute_rr_vars();
    int update_time_rr(int gate_id, int &time);
    int one_bend_check_time(int gate_id, int time);
    int set_one_bend_junction(int gate_id, int jx, int jy);
    int update_time_1bp(int gate_id, int &time);
    int check_routing_conflicts();
};

class MapAlgo{
  private:
    Circuit *pCircuit;
    Machine *pMachine;
    IntSet VertexList;
    vector<ProgEdge> EdgeList;

    //qubit mappings
    IntVect qx;
    IntVect qy;

  public:
    MapAlgo(Circuit *pC, Machine *pM);
    void create_program_graph(Circuit *pC);
    void program_id_mapping();
    void copy_mapping(SMTOutput *);
};

class QParams{
  public:
    int maxTimeSlot;
    int CNOT_Count_Max;
    int time_CNOT;
    int time_X;
    int time_Y;
    int time_H;
    int time_MeasZ;
    int machineRows;
    int machineCols;
    int routingPolicy;
    int couplingThresh;
    int transitiveClFlag;
    int dataAwareFlag;
    int is_greedy;
};
extern QParams gParams;

class QStats{
  public:
    int cntMapping;
    int cntDepend;
    int cntRouting;

    int z3_arith_conflicts;
    int z3_conflicts;
    int z3_arith_add_rows;
    int z3_mk_clause;
    int z3_added_eqs;
    int z3_binary_propagations;
    int z3_restarts;
    int z3_decisions;
}; 

extern QStats gStats;

void test();
int MyGetTime();
void print_gd(int id, int g, int d, string gate_name);
void accumulate_stats(stats &S);
void init_stats();
void print_stats();


#endif
