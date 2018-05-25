import sys
from llvm_parser import LLVMInputParser
from pre_process import PreprocessGates
import networkx as nx
from networkx.drawing.nx_agraph import graphviz_layout
from networkx.drawing.nx_agraph import write_dot
import matplotlib.pyplot as plt
import numpy as np
from operator import itemgetter
from ingest_mdata import IngestMachineData

software_gates = ["Z", "S", "Sdag", "T", "Tdag", "PrepZ"]

class PostProcessSchedule:
    def __init__(self, G_full, G_pp, schedule_file, routing_policy, data_aware):
        self.G_full = G_full 
        self.G_pp = G_pp
        self.schedule_file = schedule_file
        self.qmap = {}
        self.q2mq = {}
        self.gate_times = {}
        self.routes = {}

        if routing_policy == "onebend":
            self.is_one_bend = True
        else:
            self.is_one_bend = False

        if data_aware == "data":
            self.data_aware = True
        else:
            self.data_aware = False

        self.newGateIdx = 0

    def parse_schedule(self):
        ifile = open(self.schedule_file, 'r')
        l = ifile.readlines()
        for line in l:
            sline = line.split()
            if "Q" in line:
                self.qmap[int(sline[1])] = [int(sline[2]), int(sline[3])]
                self.q2mq[int(sline[1])] = self.machine_map[sline[2] + " " + sline[3]]
                #print int(sline[1]), "goes to", self.q2mq[int(sline[1])]
            if "G" in line:
                self.gate_times[int(sline[1])] = [int(sline[2]), int(sline[3])]
            if "R" in line:
                self.routes[int(sline[1])] = [int(sline[2]), int(sline[3])]
                self.is_one_bend = True
        #print self.q2mq

    def get_gates_of_one_qubit(self, G, qubit):
        # get set of gates of this qubit
        # get first gate, do DFS
        set_of_gates = [x for x in G.nodes if qubit in G.nodes[x]['details']['qubits']]
        subG = G.subgraph(set_of_gates)
        startGate = [x[0] for x in subG.in_degree(subG.nodes) if x[1] == 0]
        endGate = [x[0] for x in subG.out_degree(subG.nodes) if x[1] == 0]
        assert len(startGate) == 1
        assert len(endGate) == 1
        #for item in nx.dfs_edges(subG, startGate[0]):
        #    print item, subG.nodes[item[0]]['details']['type'], subG.nodes[item[1]]['details']['type']
        return [item for item in nx.dfs_edges(subG, startGate[0])], endGate[0]

    def add_timings_to_unprocessed_graph(self):
        self.G_time = self.G_full.copy()
        self.maxT = -1
        for gate_id in self.gate_times.keys():
            old_gate_id = self.G_pp.nodes[gate_id]['prev_id']
            stime = self.gate_times[gate_id][0]
            duration = self.gate_times[gate_id][1]
            self.G_time.nodes[old_gate_id]['details']['startTime'] = stime
            self.G_time.nodes[old_gate_id]['details']['duration'] = duration
            self.maxT = max(self.maxT, stime+duration+1)

    def compute_timings_for_software_gates(self):
        nqubits = len(self.qmap.keys())
        for q in range(nqubits):
            gate_dependL, endG = self.get_gates_of_one_qubit(self.G_time, q)
            #if endG is software, assign it the last timeslot
            gate_dependL.reverse()
            if self.G_time.nodes[endG]['details']['type'] in software_gates:
                self.G_time.nodes[endG]['details']['startTime'] = self.maxT
                self.G_time.nodes[endG]['details']['duration'] = 0
            for e in gate_dependL:
                g1 = e[0]
                g2 = e[1]
                # g1 -> g2
                dt1 = self.G_time.nodes[g1]['details']
                dt2 = self.G_time.nodes[g2]['details']
                assert (dt2['startTime'] >= 0)
                if dt1['type'] in software_gates:
                    dt1['startTime'] = dt2['startTime']
                    dt1['duration'] = 0
                else:
                    assert (dt1['startTime'] >= 0)
        for node_id in self.G_time.nodes:
            assert self.G_time.nodes[node_id]['details']['startTime'] >= 0

    def setup_routing_phase(self):
        self.G_hw = self.G_time.copy()
        # change qubit names from program qubit name to the corresponding h/w qubit
        ghw = self.G_hw
        for gate_id in ghw:
            dt = ghw.nodes[gate_id]['details']
            qubits = dt['qubits']
            mapped_qubits = [self.q2mq[qubits[i]] for i in range(len(qubits))] 
            dt['hwqubits'] = mapped_qubits
            labelStr = str(gate_id) + ":" +  dt['type']
            for q in dt['hwqubits']:
                labelStr += str(q) + ' '
            ghw.nodes[gate_id]['label'] = labelStr
        self.newGateIdx = len(ghw.nodes()) + 1 #ids for new swap gates and cnots
        write_dot(self.G_hw, "G_preroute.dot")

    def find_path_source_to_entry(self, G, junction, dest, prefix=[]):
        hwtarg = dest
        targ_entry = self.mGraph.neighbors(hwtarg) #entry points to the destination, find best one
        req_tentry = -1
        req_tentry_cost = float('inf')
        req_path = None
        for tentry in targ_entry:
            one_path = nx.shortest_path(self.mGraph, junction, tentry)
            num_swaps = 0
            if len(one_path) > 0:
                num_swaps += len(one_path) - 1
            if len(prefix) > 0:
                num_swaps += len(prefix) - 1
            cost = num_swaps*2*self.mGateTime["SWAP"]
            last_hop_cost = 0
            if hwtarg in self.mDigraph[tentry]: #cnot from tentry->hwtarg
                last_hop_cost = self.mGateTime["CNOT"]
            elif tentry in self.mDigraph[hwtarg]: #cnot from hwtarg->centry
                last_hop_cost = self.mGateTime["CNOT"] + self.mGateTime["H"]*2
            else:
                assert (0)
            cost = cost + last_hop_cost
            if cost < req_tentry_cost:
                req_tentry_cost = cost
                req_tentry = tentry
                req_path = one_path
        #we have determined the junction->dest shortest path now
        #prepend it with the prefix
        #req new cnot is (tentry, hwtarg)
        req_path = prefix + req_path
        return req_path, [req_tentry, hwtarg], req_tentry_cost
 
    def _connect_nodes_all_pairs(self, G, list1, list2):
        #print "Connecting", [i for i in list1], [j for j in list2]
        for item1 in list1:
            for item2 in list2:
                G.add_edge(item1, item2)
   
    def _add_swap_gate(self, G, q1, q2, start, cnotID):
        duration = self.mGateTime["SWAP"]
        labelStr = str(self.newGateIdx) + ":" +  "SWAP(" + str(cnotID) + ")" +  str(q1) + " " + str(q2)
        G.add_node(self.newGateIdx, details = {'type': "SWAP", 'hwqubits':[q1, q2], 'startTime':start, 'duration':duration, 'cnotID':cnotID}, label=labelStr)
        self.newGateIdx += 1
        return self.newGateIdx-1, start+duration

    def _add_cnot(self, G, ctrl, target, start):
        duration = self.mGateTime["CNOT"]
        labelStr = str(self.newGateIdx) + ":" + "CNOT " + str(ctrl) + " " + str(target)
        G.add_node(self.newGateIdx, details = {'type': "CNOT", 'hwqubits':[ctrl, target], 'startTime':start, 'duration':duration}, label=labelStr)
        self.newGateIdx += 1
        return self.newGateIdx-1, start+duration 

    def _add_hadamard(self, G, q, start):
        duration = self.mGateTime["H"]
        labelStr = str(self.newGateIdx) + ":" + "H " + str(q) 
        G.add_node(self.newGateIdx, details = {'type': "H", 'hwqubits':[q], 'startTime':start, 'duration':duration}, label=labelStr)
        self.newGateIdx += 1
        return self.newGateIdx-1, start+duration

    def _add_chain_of_swaps(self, G, npath, parents, start, cnotID):
        insGateList = []
        lastSwapGate = []
        ctime = start
        for i in range(len(npath)-1):
            gate_id, ctime = self._add_swap_gate(G, npath[i], npath[i+1], ctime, cnotID)
            insGateList.append(gate_id)
        # connect parents to first swap, and other swaps consecutively
        if len(insGateList) > 0:
            self._connect_nodes_all_pairs(G, parents, [insGateList[0]])
            for i in range(len(insGateList)-1):
                G.add_edge(insGateList[i], insGateList[i+1]) 
            lastSwapGate.append(insGateList[-1])
            return lastSwapGate, ctime
        else:
            return parents, ctime
    
    def _add_direction_aware_cnot(self, G, ctrl, target, parents, ctime):
        lastGateList = []
        if target in self.mDigraph[ctrl]:
            gate_id, ctime = self._add_cnot(G, ctrl, target, ctime)
            self._connect_nodes_all_pairs(G, parents, [gate_id])
            lastGateList = [gate_id]
        elif ctrl in self.mDigraph[target]:
            h11, tmp          = self._add_hadamard(G, ctrl, ctime)
            h21, ctime        = self._add_hadamard(G, target, ctime)
            cnotgate, ctime   = self._add_cnot(G, target, ctrl, ctime)
            h12, tmp          = self._add_hadamard(G, ctrl, ctime)
            h22, ctime        = self._add_hadamard(G, target, ctime)
            self._connect_nodes_all_pairs(G, [h11, h21], [cnotgate])
            self._connect_nodes_all_pairs(G, [cnotgate], [h12, h22])
            self._connect_nodes_all_pairs(G, parents, [h11, h21])
            lastGateList = [h12, h22]
        else:
            assert (0)
        return lastGateList, ctime

    def add_cnot_routing_gates(self, pRouteData, pRouteNoData):
        ghw = self.G_hw
        cnotIDs = []
        for gate_id in ghw.nodes:
            dt = ghw.nodes[gate_id]['details']
            if not dt['type'] == "CNOT":
                continue
            cnotIDs.append(gate_id)
        
        routePath = None
        routeCost = None
        if self.is_one_bend:
            if self.data_aware:
                routePath = pRouteData.onebend_path_map
                routeCost = pRouteData.onebend_cost_map
            else:
                routePath = pRouteNoData.onebend_path_map
                routeCost = pRouteNoData.onebend_cost_map
        else:
            if self.data_aware:
                routePath = pRouteData.rr_path_map
                routeCost = pRouteData.rr_cost_map
            else:
                routePath = pRouteNoData.rr_path_map
                routeCost = pRouteNoData.rr_cost_map


        for gate_id in cnotIDs: 
            dt = ghw.nodes[gate_id]['details']
            hwctrl = dt['hwqubits'][0]
            hwtarg = dt['hwqubits'][1]
            if not self.is_one_bend:
                npath = routePath[hwctrl][hwtarg]['path']
                ncnot = routePath[hwctrl][hwtarg]['hwcnot']
                cost = routeCost[hwctrl][hwtarg]
            if self.is_one_bend:
                junct = self.routes[gate_id]
                junct_name = str(junct[0]) + " " + str(junct[1])
                hwjunct = self.machine_map[junct_name]

                #quick fix for a corner case: this occurs because for the no data case. when the control and target on a line, i.e. the rectangle collapses to a line, for the data aware case, only one path is allowed..so only of the junction entries is present in the routing data structure..how to fix cleanly? when control and target are on a line, make the solver forbid the right junction
                if routeCost[hwctrl][hwtarg][hwjunct] == float('inf'):
                    for junction in routeCost[hwctrl][hwtarg].keys():
                        if junction != hwjunct:
                            hwjunct = junction
                            #print 'resetting junction to', hwjunct
                            break

                npath = routePath[hwctrl][hwtarg][hwjunct]['path']
                ncnot = routePath[hwctrl][hwtarg][hwjunct]['hwcnot']
                cost = routeCost[hwctrl][hwtarg][hwjunct]
                #print hwctrl, hwtarg, npath,ncnot, hwjunct, cost
            #assert (cost >= dt['duration'] + 1) #1 adjustment is our assumption throughout the code
            #assert (cost <= dt['duration'] + 3) #at most two more hadamard gates for the actual CNOT's direction issue
            #print 'Cost check:', npath, ncnot, cost, dt['duration']
            # adjust start times of descendants if necessary - create the required gap
            # adjust this CNOT's duration
            tdiff = cost - dt['duration'] - 1
            if tdiff > 0:
                descendL = nx.descendants(ghw, gate_id)
                for did in descendL:
                    dt_descend = ghw.nodes[did]['details']
                    dt_descend['startTime'] += tdiff
            dt['duration'] = dt['duration'] + tdiff

            # route this cnot and modify dependencies
            parentsL = [p for p in ghw.predecessors(gate_id)]
            childrenL =[c for c in ghw.successors(gate_id)]
            ctime0 = dt['startTime']
            connect1, ctime1 = self._add_chain_of_swaps(ghw, npath, parentsL, ctime0, gate_id)
            connect2, ctime2 = self._add_direction_aware_cnot(ghw, ncnot[0], ncnot[1], connect1, ctime1)
            npath.reverse()
            connect3, ctime3 = self._add_chain_of_swaps(ghw, npath, connect2, ctime2, gate_id)
            self._connect_nodes_all_pairs(ghw, connect3, childrenL)
            ghw.remove_node(gate_id)
            #TODO
            #assert (dt['duration'] + 1 == (ctime3 - ctime0)) #gate finished in prev slot
        write_dot(self.G_hw, "G_swaps.dot")

    def tranform_swaps_to_cnots(self):
        ghw = self.G_hw
        swapIDs = []
        for gate_id in ghw.nodes:
            dt = ghw.nodes[gate_id]['details']
            if not dt['type'] == "SWAP":
                continue
            swapIDs.append(gate_id)
        for gate_id in swapIDs:
            dt = ghw.nodes[gate_id]['details']
            q1 = dt['hwqubits'][0]
            q2 = dt['hwqubits'][1]
            if q2 in self.mDigraph[q1]: 
                #q1->q2 exists
                #swap sequence (q1->q2), (q2->q1), (q1->q2)
                newQ1 = q1
                newQ2 = q2
            elif q1 in self.mDigraph[q2]:
                #q2->q1 exists
                #swap sequence (q2->q1), (q1->q2), (q2->q1)
                newQ1 = q2
                newQ2 = q1
            parentsL = [p for p in ghw.predecessors(gate_id)]
            childrenL =[c for c in ghw.successors(gate_id)]
            ctime0 = dt['startTime']
            connect1, ctime1 = self._add_direction_aware_cnot(ghw, newQ1, newQ2, parentsL, ctime0)
            connect2, ctime2 = self._add_direction_aware_cnot(ghw, newQ2, newQ1, connect1, ctime1)
            connect3, ctime3 = self._add_direction_aware_cnot(ghw, newQ1, newQ2, connect2, ctime2)
            self._connect_nodes_all_pairs(ghw, connect3, childrenL)
            ghw.remove_node(gate_id)
            assert (ctime3-ctime0 == dt['duration'])
        write_dot(self.G_hw, "G_final.dot")

    def generate_qasm_code(self, measure_at_end=True):
        max_time_slot = -1
        ghw = self.G_hw

        for gate_id in ghw.nodes:
            dt = ghw.nodes[gate_id]['details']
            dt['measure_list'] = []
            max_time_slot = max(max_time_slot, dt['startTime'] + dt['duration'])
            hwq = dt['hwqubits']

        #print max_time_slot
        qasm_targets = []
        
        for gate_id in ghw.nodes:
            dt = ghw.nodes[gate_id]['details']
            qasm_targets.append([dt['startTime'], dt])
        self.qasm_sorted = sorted(qasm_targets, key=itemgetter(0))
    
        measure_map = {}
        for q in self.q2mq.keys():
            prog_q = q
            mach_q = self.q2mq[prog_q]
            measure_map[mach_q] = prog_q

        self.qasm_code = []
        for item in self.qasm_sorted:
            dt = item[1]
            one_line = ''
            skip = False
            if dt['type'] == 'H':
                one_line += 'h '
            elif dt['type'] == 'X':
                one_line += 'x '
            elif dt['type'] == 'Y':
                one_line += 'y '
            elif dt['type'] == 'Z':
                one_line += 'z '
            elif dt['type'] == 'CNOT':
                one_line += 'cx '
            elif dt['type'] == 'S':
                one_line += 's '
            elif dt['type'] == 'Sdag':
                one_line += 'sdg '
            elif dt['type'] == 'T':
                one_line += 't '
            elif dt['type'] == 'Tdag':
                one_line += 'tdg '
            #elif dt['type'] == 'MeasZ':
            #    one_line += 'measure '
            else:
                skip = True
                #print 'skipping', dt

            if not skip:
                hwq = dt['hwqubits']
                if len(hwq) == 1:
                    if not "measure" in one_line:
                        one_line += 'q['+str(hwq[0]) + '];'
                    #if "measure" in one_line:
                    #    one_line += 'q['+str(hwq[0]) + '] -> c[' + str(measure_map[hwq[0]]) + '];' 
                elif len(hwq) == 2:
                    one_line += 'q['+str(hwq[0]) + '], q[' + str(hwq[1]) + '];'
                self.qasm_code.append(one_line + "\n")
        self.qasm_head = "OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q[16];\ncreg c[16];\n"
        self.qasm_tail = ""
        if measure_at_end:
            for q in self.q2mq.keys():
                prog_q = q
                mach_q = self.q2mq[prog_q]
                self.qasm_tail += "measure q[" + str(mach_q) + "] -> c[" + str(prog_q) + "];\n"
            
    def print_qasm_code(self, qasmfilename, mapfilename):
        f = open(qasmfilename, 'w')
        outstr = ""
        outstr += self.qasm_head
        for line in self.qasm_code:
            outstr += line
            #print line
        outstr += self.qasm_tail
        f.write(outstr)
        f.close()

        f = open(mapfilename, 'w')
        for q in self.q2mq.keys():
            prog_q = q
            mach_q = self.q2mq[prog_q]
            f.write(str(mach_q) + ' ' + str(prog_q) + '\n')
        f.close()
        
    def setup_ibm_machine_configuration(self):
        self.machine_map = {}
        self.machine_map["0 0"] = 1 
        self.machine_map["0 1"] = 2
        self.machine_map["0 2"] = 3
        self.machine_map["0 3"] = 4
        self.machine_map["0 4"] = 5
        self.machine_map["0 5"] = 6
        self.machine_map["0 6"] = 7
        self.machine_map["0 7"] = 8
        self.machine_map["1 7"] = 9
        self.machine_map["1 6"] = 10
        self.machine_map["1 5"] = 11
        self.machine_map["1 4"] = 12
        self.machine_map["1 3"] = 13
        self.machine_map["1 2"] = 14
        self.machine_map["1 1"] = 15
        self.machine_map["1 0"] = 0 
        self.machine_cnot = {1:[0,2], 2:[3], 3:[4, 14], 5:[4], 6:[5,7,11], 7:[10], 8:[7],9:[8, 10], 11:[10], 12:[5, 11, 13], 13:[4, 14], 15:[0, 2, 14]}
        self.machine_num_qubits = 16
        self.mGateTime = {"H":1, "X":2, "Y":2, "CNOT":8}
        self.mGateTime["SWAP"] = self.mGateTime["CNOT"]*3 + self.mGateTime["H"]*2
        
        '''
        # added for test
        self.machine_map["0 8"] = 16
        self.machine_map["1 8"] = 17
        self.machine_cnot[16] = [8]
        self.machine_cnot[17] = [9, 16]
        self.machine_num_qubits += 2
        '''

        self.mDigraph = nx.DiGraph()
        self.mGraph = nx.Graph()
        for i in range(self.machine_num_qubits):
            self.mGraph.add_node(i)
            self.mDigraph.add_node(i)
        for c in self.machine_cnot.keys():
            tL = self.machine_cnot[c]
            for t in tL: 
                self.mGraph.add_edge(c, t)
                self.mDigraph.add_edge(c, t)
def main():
    #parse input
    pLlvm = LLVMInputParser(sys.argv[1]) #.pyout file
    pLlvm.parse_gate_data()
    pLlvm.process_gate_dependency()
    pLlvm.process_qubit_names()
    #print pLlvm.get_qubit_mapping() 
    #do preprocess paths
    pPre = PreprocessGates(pLlvm.get_gate_info(), pLlvm.get_qubit_cnt(), software_gate_list=software_gates)
    pPre.construct_network()
    pPre.remove_software_gates()
    #run solver separately on the output file and come back python
    
    pRouteData = IngestMachineData("/home/prakash/code/opts/data/gf.txt")
    pRouteData.setup_ibm_machine_configuration()
    pRouteData.read_gf_pulse_data()
    pRouteData.compute_cost_and_path_rr()
    pRouteData.compute_cost_and_path_1bend()
    #pRouteData.compare_rr_1bend_paths()
    pRouteData.sanity_check_costs()

    pRouteNoData = IngestMachineData("/home/prakash/code/opts/data/gf_naive.txt")
    pRouteNoData.setup_ibm_machine_configuration()
    pRouteNoData.read_gf_pulse_data()
    pRouteNoData.compute_cost_and_path_rr()
    pRouteNoData.compute_cost_and_path_1bend()
    pRouteNoData.sanity_check_costs()

    #now take in the schedule
    pPost = PostProcessSchedule(pPre.G_full, pPre.G_pp, sys.argv[2], sys.argv[3], sys.argv[4])  #schedule output
    pPost.setup_ibm_machine_configuration()
    pPost.parse_schedule()
    pPost.add_timings_to_unprocessed_graph()
    pPost.compute_timings_for_software_gates()
    pPost.setup_routing_phase()     
    pPost.add_cnot_routing_gates(pRouteData, pRouteNoData)  #adds fictional swap gates
    pPost.tranform_swaps_to_cnots() #implements swap gates using CNOTs
    pPost.generate_qasm_code()      
    pPost.print_qasm_code(sys.argv[5], sys.argv[6])

if __name__=="__main__":
    main()

