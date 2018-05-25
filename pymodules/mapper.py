import sys
from llvm_parser import LLVMInputParser
import networkx as nx
from networkx.drawing.nx_agraph import write_dot
import matplotlib.pyplot as plt
from pre_process import *
from operator import itemgetter

class Mapper:
    def __init__(self):
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
        pPG.compute_program_graph(pPG.G_pp)
        self.program_graph = pPG.program_graph
        
        #print self.program_graph.nodes
        #print self.program_graph.edges
    def setup_machine(self, M, N):
        self.node_dict = {}
        node_id = 0
        self.machine_graph = nx.Graph() 
        for i in range(M):
            self.node_dict[i] = {}
            for j in range(N):
                self.node_dict[i][j] = node_id
                self.machine_graph.add_node(node_id, xloc=i, yloc=j) 
                node_id += 1

        #add vertical edges
        for i in range(M-1):
            for j in range(N):
                u = self.node_dict[i][j]
                v = self.node_dict[i+1][j]
                self.machine_graph.add_edge(u,v)

        #add horizontal edges
        for i in range(M):
            for j in range(N-1):
                u = self.node_dict[i][j]
                v = self.node_dict[i][j+1]
                self.machine_graph.add_edge(u,v)


    def compute_vertex_weights(self, M, N):
        #compute vertex weights
        pnodes = [n for n in self.program_graph.nodes]
        pnodes_wt = []
        for u in pnodes:
            my_weight = 0
            for v in self.program_graph.neighbors(u):
                wt = self.program_graph[u][v]['weight']
                my_weight += wt
            pnodes_wt.append(my_weight)
        newlist = [list(x) for x in zip(*sorted(zip(pnodes, pnodes_wt), key=itemgetter(1), reverse=True))]
        self.prog_nodes = newlist[0]
        self.prog_weight = newlist[1]
 
    def compute_mapping_cost(self, mapping, display=False):
        cost = 0
        mg = self.machine_graph
        pg = self.program_graph

        for pu in mapping.keys():
            for pv in mapping.keys():
                if pg.has_edge(pu, pv):
                    wt = pg[pu][pv]['weight']
                    u = mapping[pu]
                    v = mapping[pv]
                    edge_cost = abs(mg.node[u]['xloc'] - mg.node[v]['xloc']) + abs(mg.node[u]['yloc'] - mg.node[v]['yloc']) - 1
                    if display:
                        print pu, pv, u, v, edge_cost, wt, edge_cost*wt
                    cost += edge_cost * wt
        return cost

    def compute_cost(self, mapping, pnode_id, loc):
        cost = 0
        mg = self.machine_graph
        pnode = self.prog_nodes[pnode_id]

        for pvertex in mapping.keys():
            if self.program_graph.has_edge(pnode, pvertex):
                wt = self.program_graph[pnode][pvertex]['weight']
                v = mapping[pvertex]
                edge_cost = abs(mg.node[loc]['xloc'] - mg.node[v]['xloc']) + abs(mg.node[loc]['yloc'] - mg.node[v]['yloc']) - 1
                cost += edge_cost * wt
        return cost

    def find_best_location(self, mapping, pnode_id, locations):
        #mapping: current mapping
        #u: current vertex
        #locations: available locations
        min_loc = 0
        min_cost = float('inf')
        for l in locations:
            cost = self.compute_cost(mapping, pnode_id, l)
            #print self.prog_nodes[pnode_id], 'location:', l, 'cost:', cost
            if cost < min_cost:
                min_loc = l
                min_cost = cost
        return min_loc

    def print_program_node_prop(self, pnode_id):
        u = self.prog_nodes[pnode_id]
        for v in self.program_graph.neighbors(u):
            print u, v, self.program_graph[u][v]['weight']

    def find_one_mapping(self, start_vertex_id, start_location):
        mapping = {}
        locations = [l for l in self.machine_graph.nodes]
        #print 'Heaviest node:', self.prog_nodes[0]

        mapping[self.prog_nodes[start_vertex_id]] = start_location
        locations.remove(start_location)

        for u in range(len(self.prog_nodes)):
            if u == start_vertex_id:
                continue
            #find best location for ith program node
            pnode_id = u
            loc = self.find_best_location(mapping, pnode_id, locations) 
            mapping[self.prog_nodes[pnode_id]] = loc
            locations.remove(loc)
            
            #self.print_program_node_prop(pnode_id)
            #print 'location for', self.prog_nodes[pnode_id], ':', loc
            #print mapping
        cost = self.compute_mapping_cost(mapping)
        #print start_vertex_id, start_location, cost
        return mapping,cost

    def find_all_mappings(self):
        min_cost = float('inf')
        best_mapping = None
        #for start_vertex_id in range(len(self.program_graph.nodes)):
        for start_vertex_id in [0]:
            #print start_vertex_id
            for start_location in self.machine_graph.nodes:
                #print start_location
                mapping, cost = self.find_one_mapping(start_vertex_id, start_location)
                #print cost, mapping
                if cost < min_cost:
                    min_cost = cost
                    best_mapping = mapping
        return best_mapping, min_cost
        #self.compute_mapping_cost(best_mapping, display=True)
    
    def print_program_graph(self):
        for pnode_id in range(len(self.prog_nodes)):
            self.print_program_node_prop(pnode_id)

    def print_mapping(self, mapping):
        for u in mapping.keys():
            pos = mapping[u]
            print u, self.machine_graph.node[pos]['xloc'], self.machine_graph.node[pos]['yloc']

if __name__=="__main__":
    mapper = Mapper()
    M = int(sys.argv[2])
    N = int(sys.argv[3])
    mapper.setup_machine(M, N)
    mapper.compute_vertex_weights(M,N)
    mapping, cost = mapper.find_all_mappings()
    mapper.print_mapping(mapping)
    #mapper.print_program_graph()

     

