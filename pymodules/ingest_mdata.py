import networkx as nx

class IngestMachineData:
    def __init__(self, GF_Pulse_File):
        self.GFPulseFile = GF_Pulse_File
    
    def _id2ij(self, qid):
        locStr = self.qid2loc[qid]
        sploc = locStr.split()
        return int(sploc[0]), int(sploc[1])
    
    def _ij2id(self, i, j):
        locStr = str(i) + " " + str(j)
        return self.machine_map[locStr]

    def _point_in_rect(self, lx, ly, rx, ry, px, py):
        return (px >= lx and px <= rx) and (py >= ly and py <= ry)

    def _get_lr_rect(self, cx, cy, tx, ty):
        lx = min(cx, tx)
        ly = min(cy, ty)
        rx = max(cx, tx)
        ry = max(cy, ty)
        return lx, ly, rx, ry
    
    def _get_junctions(self, cx, cy, tx, ty):
        j1x = cx
        j1y = ty
        j2x = tx
        j2y = cy
        return j1x, j1y, j2x, j2y

    def setup_ibm_machine_configuration(self):
        self.machine_map = {}
        self.mrows = 2
        self.mcols = 8
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
        self.qid2loc = {}
        for loc in self.machine_map.keys():
            self.qid2loc[self.machine_map[loc]] = loc

    def read_gf_pulse_data(self):
        self.GD_pulse_time = 80
        f = open(self.GFPulseFile, 'r')
        self.gf_pulse_t = {}
        l = f.readlines()
        self.swap_graph = nx.Graph()
        self.unweighted_graph = nx.Graph()
        self.cnot_graph = nx.DiGraph()
        for line in l:
            sline = line.split()
            q0 = int(sline[0])
            q1 = int(sline[1])
            gf_t = int(sline[2])
            cnot_time = float(gf_t + self.GD_pulse_time)*2/self.GD_pulse_time   # ONE CNOT
            swap_time = 3*cnot_time + 2                                         # ONE SWAP

            self.swap_graph.add_edge(q0, q1, weight=round(swap_time, 0))  
            self.cnot_graph.add_edge(q0, q1, weight=round(cnot_time, 0))
            self.unweighted_graph.add_edge(q0, q1)

    def compute_cost_and_path_rr(self):
        # compute time for all pairs of cnots
        # to go from i to j, go to an entry point with swaps, then find cnot cost from entry to target including direction
        self.rr_cost_map = {}
        self.rr_path_map = {}
        num_nodes = len(self.swap_graph.nodes)
        for i in range(num_nodes):
            self.rr_cost_map[i] = {}
            self.rr_path_map[i] = {}
            for j in range(num_nodes): 
                # we are doing i to j
                control = i
                target = j
                if control == target:
                    #print self.qid2loc[i], self.qid2loc[j], 0
                    self.rr_cost_map[i][j] = float('inf')
                    self.rr_path_map[i][j] = []
                    continue

                cx, cy = self._id2ij(control)
                tx, ty = self._id2ij(target)
                lx, ly, rx, ry = self._get_lr_rect(cx, cy, tx, ty)
                tentryList = self.swap_graph.neighbors(j)
                req_tentry = -1
                req_tentry_cost = float('inf')
                req_path = None

                for tentry in tentryList:
                    tex, tey = self._id2ij(tentry)
                    #eligibility criteria: should be within the c,t rectangle - this is how we abide by the geometry
                    pt_valid = self._point_in_rect(lx, ly, rx, ry, tex, tey)    
                    if not pt_valid:
                        continue

                    spath = nx.dijkstra_path(self.swap_graph, control, tentry)
                    edge_list = zip(spath[1:],spath[:-1]) 
                    swap_cost = 0
                    for item in edge_list:
                        swap_cost += self.swap_graph.edges[item]['weight']
                    cost = swap_cost*2
        
                    if target in self.cnot_graph[tentry]: #cnot from tentry->target
                        cost += self.cnot_graph[tentry][target]['weight']
                    elif tentry in self.cnot_graph[target]: #cnot from target->tentry
                        cost += self.cnot_graph[target][tentry]['weight'] + 2
                    else:
                        assert (0)
                    if cost < req_tentry_cost:
                        req_tentry_cost = cost
                        req_tentry = tentry
                        req_path = spath
                #print self.qid2loc[i], self.qid2loc[j], int(req_tentry_cost)
                self.rr_cost_map[i][j] = int(req_tentry_cost) #cost includes cnot cost also, path does not
                self.rr_path_map[i][j] = {'path':req_path, 'hwcnot': (req_tentry, target)} #path, hwcnot 

    def find_path_source_to_entry(self, G, junction, dest, prefix=[], prefix_cost=0):
        hwtarg = dest
        targ_entry = self.unweighted_graph.neighbors(hwtarg) #entry points to the destination, find best one

        req_tentry = -1
        req_tentry_cost = float('inf')
        req_path = None
        
        jx, jy = self._id2ij(junction)
        dx, dy = self._id2ij(dest)
        lx, ly, rx, ry = self._get_lr_rect(jx, jy, dx, dy)

        for tentry in targ_entry:
            #eligibility criteria: should be within the rectangle
            tex, tey = self._id2ij(tentry)
            pt_valid = self._point_in_rect(lx, ly, rx, ry, tex, tey)    
            if not pt_valid:
                continue
            one_path = nx.shortest_path(self.unweighted_graph, junction, tentry)
            spath = prefix
            if len(one_path) > 0:
                spath += one_path[1:]

            edge_list = zip(spath[1:],spath[:-1]) 
            swap_cost = 0
            flag = 0
            for item in edge_list:
                swap_cost += self.swap_graph.edges[item]['weight']
                flag = 1
            swap_cost *= 2

            last_hop_cost = 0
            if hwtarg in self.cnot_graph[tentry]: #cnot from tentry->hwtarg
                last_hop_cost = self.cnot_graph[tentry][hwtarg]['weight']
                flag = 1
            elif tentry in self.cnot_graph[hwtarg]: #cnot from hwtarg->centry
                last_hop_cost = self.cnot_graph[hwtarg][tentry]['weight'] + 2
                flag = 1
            else:
                assert (0)
            
            if flag == 1:
                cost = swap_cost + last_hop_cost
                if cost < req_tentry_cost:
                    req_tentry_cost = cost
                    req_tentry = tentry
                    req_path = spath
            
        return req_tentry_cost, {'path':req_path, 'hwcnot':(req_tentry, hwtarg)}

    def _process_one_junction(self, C, T, L, R, J, control, target, junction):
        # compute c -> j, j->tentry, tentry->t costs, and reverse swaps
        cx, cy = C[0], C[1]
        tx, ty = T[0], T[1]
        lx, ly = L[0], L[1]
        rx, ry = R[0], R[1]
        jx, jy = J[0], J[1]

        cj_path = nx.shortest_path(self.unweighted_graph, control, junction)
        cost, path = self.find_path_source_to_entry(self.unweighted_graph, junction, target, cj_path, 0)
        return cost, path

    def compute_cost_and_path_1bend(self):
        # for each cnot, determine the two junctions
        self.onebend_cost_map = {}
        self.onebend_path_map = {}
        num_nodes = len(self.swap_graph.nodes)
        for i in range(num_nodes):
            self.onebend_cost_map[i] = {}
            self.onebend_path_map[i] = {}
            for j in range(num_nodes): 
                # we are doing i to j
                control = i
                target = j
                if control == target:
                    #print self.qid2loc[i], self.qid2loc[j], self.qid2loc[i], 0
                    jid1 = -1
                    jid2 = -2
                    c1 = float('inf')
                    c2 = float('inf')
                    p1 = {}
                    p2 = {}
                else:
                    cx, cy = self._id2ij(control)
                    C = [cx, cy]
                    tx, ty = self._id2ij(target)
                    T = [tx, ty]
                    lx, ly, rx, ry = self._get_lr_rect(cx, cy, tx, ty)
                    L = [lx, ly]
                    R = [rx, ry]
                    j1x, j1y, j2x, j2y = self._get_junctions(cx, cy, tx, ty)
                    J1 = [j1x, j1y]
                    J2 = [j2x, j2y]
                    jid1 = self._ij2id(j1x, j1y)
                    jid2 = self._ij2id(j2x, j2y)
                    c1, p1 = self._process_one_junction(C, T, L, R, J1, control, target, jid1)
                    c2, p2 = self._process_one_junction(C, T, L, R, J2, control, target, jid2)
                self.onebend_cost_map[i][j] = {}
                self.onebend_path_map[i][j] = {}
                self.onebend_cost_map[i][j][jid1] = c1
                self.onebend_cost_map[i][j][jid2] = c2
                self.onebend_path_map[i][j][jid1] = p1
                self.onebend_path_map[i][j][jid2] = p2


    def compare_rr_1bend_paths(self):
        fdump = []
        for i in self.rr_cost_map.keys():
            for j in self.rr_cost_map[i].keys():
                if i == j:
                    continue
                control = i
                target = j
                rrcost = self.rr_cost_map[i][j]
                onebendcost = []
                onebendfactors = []
                for jid in self.onebend_cost_map[i][j].keys():
                    tmp = self.onebend_cost_map[i][j][jid]
                    if tmp != float('inf'):
                        onebendcost.append(tmp)
                        onebendfactors.append(tmp/rrcost)
                fdump.append(min(onebendfactors))
        print min(fdump), max(fdump)

    def print_rr_costs(self):
        f = open("../data/cnot_rr_time.txt", 'w')
        for i in self.rr_cost_map.keys():
            for j in self.rr_cost_map[i].keys():
                if i == j:
                    writeStr = self.qid2loc[i] + ' ' + self.qid2loc[j] + ' ' + '-1' + '\n' 
                else:
                    writeStr = self.qid2loc[i] + ' ' + self.qid2loc[j] + ' ' +  str(int(self.rr_cost_map[i][j])) + '\n'
                f.write(writeStr)
        f.close()
 
    def print_1bend_costs(self):
        f = open("../data/cnot_1bend_time.txt", 'w')
        for i in self.onebend_cost_map.keys():
            for j in self.onebend_cost_map[i].keys():
                for jid in self.onebend_cost_map[i][j].keys():
                    if i == j:
                        writeStr = self.qid2loc[i] + ' ' + self.qid2loc[j] + ' ' + self.qid2loc[i] + ' -1' + '\n' 
                    else:
                        cost = self.onebend_cost_map[i][j][jid]
                        if cost == float('inf'):
                            costStr = "-1"
                        else:
                            costStr = str(int(cost))
                        writeStr = self.qid2loc[i] + ' ' + self.qid2loc[j] + ' ' +  self.qid2loc[jid] + ' ' + costStr + '\n'
                    f.write(writeStr)
        f.close()
    
    def _one_path_cost(self, target, path):
        edge_list = zip(path[1:],path[:-1]) 
        swap_cost = 0
        flag = 0
        for item in edge_list:
            swap_cost += self.swap_graph.edges[item]['weight']
            flag = 1
        swap_cost *= 2
        tentry = path[-1]
        hwtarg = target

        last_hop_cost = 0
        if hwtarg in self.cnot_graph[tentry]: #cnot from tentry->hwtarg
            last_hop_cost = self.cnot_graph[tentry][hwtarg]['weight']
            flag = 1
        elif tentry in self.cnot_graph[hwtarg]: #cnot from hwtarg->centry
            last_hop_cost = self.cnot_graph[hwtarg][tentry]['weight'] + 2
            flag = 1
        else:
            assert (0)
        return swap_cost + last_hop_cost

    def sanity_check_costs(self):
        for i in self.rr_cost_map.keys():
            for j in self.rr_cost_map[i].keys():
                if i != j:
                    assert self.rr_cost_map[i][j] ==  self._one_path_cost(j, self.rr_path_map[i][j]['path'])

        for i in self.onebend_cost_map.keys():
            for j in self.onebend_cost_map[i].keys():
                if i != j:
                    for jid in self.onebend_cost_map[i][j].keys():
                        if self.onebend_cost_map[i][j][jid] != float('inf'): 
                            assert self.onebend_cost_map[i][j][jid] == self._one_path_cost(j, self.onebend_path_map[i][j][jid]['path']) 
         
if __name__=="__main__":
    pD = IngestMachineData("/home/prakash/code/opts/data/gf.txt")
    pD.setup_ibm_machine_configuration()
    pD.read_gf_pulse_data()
    pD.compute_cost_and_path_rr()
    pD.compute_cost_and_path_1bend()
    pD.compare_rr_1bend_paths()
    pD.sanity_check_costs()
    pD.print_rr_costs()
    pD.print_1bend_costs()
