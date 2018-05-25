import sys
import re

def tryint(s):
    try:
        return int(s)
    except:
        return s

def alphanum_key(s):
    return [ tryint(c) for c in re.split('([0-9]+)', s) ]

def sort_nicely(l):
    l.sort(key=alphanum_key)

class LLVMInputParser:
    def __init__(self, inFileName):
        self.gateInfo = {}
        self.dataFile = inFileName
        self.gLlvmId2IdMap = {} #map of gate LLVM id to python id 
        self.gateIdIdx = 0
        self.gateIDMap = {} # to take care of missing gate ids in input file
        self.qubitMap = {}
        self.qubitIdIdx = 0

    def parse_gate_data(self):
        inF = open(self.dataFile, 'r')
        inData = inF.readlines()
        for line in inData:
            self.parse_line(line)
            
    def parse_line(self, inLine):
        '''
        Input format
        2 llvm.CNOT.i16.i16
        2 b0 a0 
        2 INST_LABEL: 0x1a9fb18
        2 In_Edges: 0x1aa7f90 
        2 Out_Edges: 0x1a9ed80 0x1ab3ef8 
        '''
        if not inLine.strip():
            return
        sLine = inLine.split()
        givenGateId = int(sLine[0])
        if not givenGateId in self.gateIDMap.keys():
            self.gateIDMap[givenGateId] = self.gateIdIdx
            self.gateIdIdx += 1
        gateId = self.gateIDMap[givenGateId]
        if not gateId in self.gateInfo.keys():
            self.gateInfo[gateId] = {}
        thisGate = self.gateInfo[gateId]
        if "llvm" in inLine:
            thisGate["type"] = sLine[1].split('.')[1]
        elif "INST_LABEL" in inLine:
            thisGate["llvmId"] = sLine[2]
            self.gLlvmId2IdMap[thisGate["llvmId"]] = int(gateId)
        elif "In_Edges" in inLine:
            tmp = sLine[2:]
            thisGate["inEdges"] = tmp
        elif "Out_Edges" in inLine:
            tmp = sLine[2:]
            thisGate["outEdges"] = tmp
        else: #this is the line with the input qubits
            thisGate["qubits"] = sLine[1:]
            thisGate["startTime"] = -1
            
    def print_one_gate(self, gateId):
        thisGate = self.gateInfo[gateId]
        print "Gate ", gateId
        print "Type:", thisGate["type"]
        print "LLVM Id:", thisGate["llvmId"]
        print "In:", thisGate["inEdges"]
        print "Out:", thisGate["outEdges"]
        print "Qubits:", thisGate["qubits"]
        print "\n",

    def print_for_cpp(self, gateId):
        thisGate = self.gateInfo[gateId]
        print gateId, thisGate["type"], len(thisGate["qubits"]), 
        for i in range(len(thisGate["qubits"])):
            print thisGate["qubits"][i],
        print len(thisGate["inEdges"]),
        for i in range(len(thisGate["inEdges"])):
            print thisGate["inEdges"][i],
        #print len(thisGate["outEdges"]),
        #for i in range(len(thisGate["outEdges"])):
        #    print thisGate["outEdges"][i],
        print ""

    def print_all_for_cpp(self):
        print self.qubitIdIdx, self.gateIdIdx
        for i in self.gateInfo.keys():
            self.print_for_cpp(i)

    def print_all_gates(self):
        for i in self.gateInfo.keys():
            self.print_one_gate(i)

    def _gate_llvmid2id(self, llvmId):
        return self.gLlvmId2IdMap[llvmId]

    def _replace_by_id(self, edgeList):
        outlist = []
        for j in range(len(edgeList)):
            tmpitem = edgeList[j]
            reqid = self._gate_llvmid2id(tmpitem)
            outlist.append(reqid)
        return outlist

    def process_gate_dependency(self):
        for i in self.gateInfo.keys():
            thisGate = self.gateInfo[i]
            newInList = self._replace_by_id(thisGate["inEdges"])
            newOutList = self._replace_by_id(thisGate["outEdges"])
            thisGate["inEdges"] = newInList
            thisGate["outEdges"] = newOutList

    def process_qubit_names(self):
        qubitNames = []
        for i in self.gateInfo.keys():
            thisGate = self.gateInfo[i]
            for j in range(len(thisGate["qubits"])):
                q = thisGate["qubits"][j]
                if not q in qubitNames:
                    qubitNames.append(q)
        sort_nicely(qubitNames)
        for q in qubitNames:
            self.qubitMap[q] = self.qubitIdIdx
            self.qubitIdIdx += 1

        for i in self.gateInfo.keys():
            thisGate = self.gateInfo[i]
            for j in range(len(thisGate["qubits"])):
                q = thisGate["qubits"][j]
                thisGate["qubits"][j] = self.qubitMap[q]
                
    def get_gate_info(self):
        return self.gateInfo
    
    def get_qubit_cnt(self):
        return self.qubitIdIdx

    def count_gates_of_type(self, gType):
        cnt = 0
        for i in self.gateInfo.keys():
            if self.gateInfo[i]["type"] == gType:
                cnt += 1
        print "Gates of type", gType, ":", cnt

    def get_qubit_mapping(self):
        return self.qubitMap

def main():
    pOS = LLVMInputParser(sys.argv[1])
    pOS.parse_gate_data()
    pOS.process_gate_dependency()
    #pOS.print_all_gates()
    pOS.process_qubit_names()
    pOS.print_all_for_cpp()
if __name__=="__main__":
    main()

