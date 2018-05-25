import sys
import subprocess as sp
import os

def get_list_of_files(path):
    return os.listdir(path)

def get_immediate_subdirectories(a_dir):
    return [name for name in os.listdir(a_dir) if os.path.isdir(os.path.join(a_dir, name))]

def prepare_qasm(path):
    #take each .out file and generate qasm
    progList = get_immediate_subdirectories(path)
    for prog in progList:
        prog_path = os.path.join(path, prog)
        files = get_list_of_files(prog_path)

        pyout_name = [f for f in files if ".pyout" in f][0]
        pyoutfile = os.path.join(prog_path, pyout_name)
        outputfiles = [f for f in files if "_ND.out" in f or "_D.out" in f]
       
        for ofile in outputfiles:
            full_ofile = os.path.join(prog_path, ofile)
            if "_ND.out" in ofile:
                qasm_path = os.path.join(prog_path, prog + "_ND.qasm")
                helper_path = os.path.join(prog_path, prog+ "_ND.map")
                print "Routing", pyoutfile, full_ofile, "no data case", qasm_path
                sp.call(["python", "pymodules/post_process.py",  pyoutfile, full_ofile, "rr", "nodata", qasm_path, helper_path])

            if "_D.out" in ofile:
                qasm_path = os.path.join(prog_path, prog + "_D.qasm")
                helper_path = os.path.join(prog_path, prog+ "_D.map")
                print "Routing", pyoutfile, full_ofile, "data case", qasm_path
                sp.call(["python", "pymodules/post_process.py",  pyoutfile, full_ofile, "onebend", "data", qasm_path, helper_path])

if __name__=="__main__":
    prepare_qasm(sys.argv[1])

