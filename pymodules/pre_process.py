import sys
from llvm_parser import LLVMInputParser
import networkx as nx
from networkx.drawing.nx_agraph import write_dot
import matplotlib.pyplot as plt

##
# G_full - original set of gates from LLVM
# G_pp   - set of gates after filtering software gates
# G_time - G_full with timings inserted
# G_hw   - starts as (G_full timings + qubit names converted to hw qubits), ends with all CNOTs routed and times adjusted 
##

class PreprocessGates:
    def __init__(self, circuit, qubitCnt, software_gate_list):
        self.gateInfo = circuit
        self.qubitCnt = qubitCnt
        self.software_gates = software_gate_list

    def construct_network(self):
        self.graph = nx.DiGraph()
        for gateId in self.gateInfo.keys():
            thisGate = self.gateInfo[gateId]
            labelStr = str(gateId) + ":" + thisGate['type'] + " "
            for q in thisGate['qubits']:
                labelStr += str(q) + " "
            self.graph.add_node(gateId, details=self.gateInfo[gateId], label=labelStr)
        for gateId in self.gateInfo.keys():
            thisGate = self.gateInfo[gateId]
            for inGate in thisGate["inEdges"]:
                self.graph.add_edge(inGate, gateId)
        self.G_full = self.graph.copy()

    def remove_software_gates(self):
        #TODO keep a copy of original graph here
        for node_id in self.gateInfo.keys(): 
            #iterating over graph nodes while modifying the graph may be errornues, hence gateInfo
            if self.graph.nodes[node_id]['details']['type'] in self.software_gates:
                # find its children, find its parents, and connect each parent to each child
                parents = [n for n in self.graph.predecessors(node_id)]
                children = [n for n in self.graph.successors(node_id)]
                for p in parents:
                    for c in children:
                        self.graph.add_edge(p, c)
                # delete this node and all its edges
                self.graph.remove_node(node_id) 
        #print 'Remaining gates:', len(list(self.graph.nodes))
        self.G_pp = nx.convert_node_labels_to_integers(self.graph, label_attribute="prev_id")
        write_dot(self.G_pp, "G_pp.dot")

    def pretty_print_graph(self, G):
        print self.qubitCnt, len(G.nodes) #qubits, gates
        for node_id in G:
            print node_id,
            thisGate = G.nodes[node_id]['details']
            print thisGate['type'],
            print len(thisGate['qubits']),
            for qubit in thisGate['qubits']:
                print qubit, 
            parents = [p for p in G.predecessors(node_id)]
            print len(parents),
            for p in parents:
                print p,
            print ""

    def pretty_print_dependencies(self, G):
        print len(G.nodes) # gates
        for node_id in G:
            print node_id,
            thisGate = G.nodes[node_id]['details']

            desc = list(nx.topological_sort(G))
            
            for i in range(len(desc)):
                if desc[i] == node_id:
                    my_desc = desc[i+1:]
                    break

            print len(my_desc),
            for d in my_desc:
                print d,
            print ""

    def print_input_graph(self):
        for node in self.G_full.nodes:
            print node, self.G_full.nodes[node]

    def compute_program_graph(self, G):
        pGraph = nx.Graph()
        self.program_graph = pGraph
        for node_id in G:
            thisGate = G.nodes[node_id]['details']
            if not "CNOT" in thisGate['type']:
                pGraph.add_node(thisGate['qubits'][0])
                continue
            qbits = thisGate['qubits']
            u = qbits[0]
            v = qbits[1]
            if pGraph.has_edge(u, v):
                edge_weight = pGraph[u][v]['weight']
            else:
                edge_weight = 0
            edge_weight += 1
            pGraph.add_edge(u, v, weight=edge_weight)

    def print_program_graph(self):
        pGraph = self.program_graph
        eId = 0
        nodeVarList = {}
        edgeList = [e for e in pGraph.edges]
        weightList = []
        for edge in edgeList: #only the undirected edges
            u = edge[0]
            v = edge[1]
            if not u in nodeVarList.keys():
                nodeVarList[u] = []
            if not v in nodeVarList.keys():
                nodeVarList[v] = []
            nodeVarList[u].append(eId)
            nodeVarList[v].append(eId)
            weightList.append(pGraph[u][v]['weight'])
            eId += 1


        lenEdges = len(edgeList)
        print lenEdges, len(nodeVarList.keys())
        for i in range(lenEdges):
            print 'E', i, edgeList[i][0], edgeList[i][1], weightList[i]

        for u in nodeVarList.keys():
            print 'N', u, len(nodeVarList[u]),
            for v in nodeVarList[u]:
                print v,
            print ""

    def compute_and_print_transitive_closure(self, G):
        transG = nx.transitive_closure(G)
        nodeCnt = len(transG.nodes)
        nodeSet = set([i for i in range(nodeCnt)])
        cnt=0
        nodecnt=0;
        for node_id in transG:
            #if this node is not a CNOT, we dont care about it
            thisGate = G.nodes[node_id]['details']
            if not "CNOT" in thisGate['type']:
                continue
            nodecnt += 1
        
        print nodecnt
        for node_id in transG:
            #if this node is not a CNOT, we dont care about it
            thisGate = G.nodes[node_id]['details']
            if not "CNOT" in thisGate['type']:
                continue
            parents = [p for p in transG.predecessors(node_id)]
            children = [c for c in transG.successors(node_id)]
            bad_set = set(parents+children)
            bad_set.add(node_id) 
            good_set = nodeSet.difference(bad_set)
            
            #prune good_set to include only CNOTs
            good_set_new = set()
            for item in good_set:
                if "CNOT" in G.nodes[item]['details']['type']:
                    if item > node_id: #symmetry breaking - add only (i,j) or (j,i) depending on value
                        good_set_new.add(item)
            good_set = good_set_new

            cnt += len(good_set)
            print node_id, len(good_set),  
            for item in list(good_set):
                print item,
            print ""
            for item in good_set:
                #check if edge exists in transitive closure :)
                assert transG.has_edge(node_id, item)==False
                assert transG.has_edge(item, node_id)==False
        
def main():
    #parse input
    pLIP = LLVMInputParser(sys.argv[1])
    pLIP.parse_gate_data()
    pLIP.process_gate_dependency()
    pLIP.process_qubit_names()
    pLIP.get_qubit_mapping()

    #do preprocess paths
    software_gates = ["Z", "S", "Sdag", "T", "Tdag", "PrepZ"]
    pPG = PreprocessGates(pLIP.get_gate_info(), pLIP.get_qubit_cnt(), software_gate_list=software_gates)
    pPG.construct_network()
    pPG.remove_software_gates()
    print_option = sys.argv[2]
    if print_option == 'preprocess':
        pPG.pretty_print_graph(pPG.G_pp) #this is what is currently used by C++
    if print_option == 'transitive':
        pPG.compute_and_print_transitive_closure(pPG.G_pp)
    if print_option == 'original':
        pPG.pretty_print_graph(pPG.G_full)
    if print_option == "dependency":
        pPG.pretty_print_dependencies(pPG.G_pp)
    if print_option == "program_graph":
        pPG.compute_program_graph(pPG.G_pp)
        pPG.print_program_graph()

    #pPG.print_input_graph() 

if __name__=="__main__":
    main()

