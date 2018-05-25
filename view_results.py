import sys
import subprocess as sp
import os
import cPickle

def get_list_of_files(path):
    return os.listdir(path)

def get_immediate_subdirectories(a_dir):
    return [name for name in os.listdir(a_dir) if os.path.isdir(os.path.join(a_dir, name))]

def load_exec_map(prog_path, backend):
    exec_id = {}
    if backend == 'qx5':
        f = open(os.path.join(prog_path, "ibmqx5.log"), 'r')
    else:
        f = open(os.path.join(prog_path, "ibmqxsim.log"), 'r')
    l = f.readlines()
    for line in l:
        sline = line.split()
        qasm_name = sline[0]
        eid = sline[1]
        exec_id[qasm_name] = eid
    f.close()
    return exec_id


def load_qubit_mapping(fname):
    f = open(fname,'r')
    l = f.readlines()
    qmap = {}
    for line in l:
        sl = line.split()
        qmap[int(sl[0])] = int(sl[1])
    return qmap

def _process_one_label(label, qmap):
    new_label = ["0"]*16
    msize = 16 #there is a reversing here - string go from 0, qubits start at machine size-1
    for mq in qmap.keys():
        new_label[(msize-1)-qmap[mq]] = label[(msize-1)-mq]
    return "".join(new_label)


def print_each_outcome(labels, values, qmap, threshold, backend):
    result_dict = {}
    for item in zip(labels, values):
        oldlabel = item[0]
        if backend == 'qx5':
            newlabel = _process_one_label(oldlabel, qmap)
        else:
            newlabel = oldlabel
        prob = item[1]
        if prob > threshold:
            print newlabel, prob
            result_dict[newlabel] = prob
    print result_dict
def print_accumulate_for_bit(labels, values, qmap, bit):
    result = {}
    for item in zip(labels, values):
        oldlabel = item[0]
        newlabel = _process_one_label(oldlabel, qmap)
        prob = float(item[1])
        bit_value = newlabel[15-bit]
        if not bit_value in result:
            result[bit_value] = prob
        else:
            result[bit_value] += prob
    print result

def get_results(path, threshold, backend):
    dirList = get_immediate_subdirectories(path)
    for d in dirList:
        print "Ignoring", d 

    prog_path = path
    files = get_list_of_files(prog_path)
    

    exec_map = load_exec_map(prog_path, backend)
    for qid in exec_map.keys():    
        print 'Results for', qid
        if backend == 'qx5':
            f = open(os.path.join(prog_path, qid.strip(".qasm") + ".pkl"), 'r')
        else:
            print 'getting results from sim pickle'
            f = open(os.path.join(prog_path, qid.strip(".qasm") + "_sim.pkl"), 'r')
        result = cPickle.load(f)
        f.close()
        qmap = load_qubit_mapping(os.path.join(prog_path, qid.strip(".qasm") + ".map"))
        print_each_outcome(result['measure']['labels'], result['measure']['values'], qmap, threshold, backend)
        print result['time_taken']

if __name__=="__main__":
    get_results(sys.argv[1], float(sys.argv[2]), sys.argv[3])
