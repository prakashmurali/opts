import sys
import subprocess as sp
import os

def get_list_of_files(path):
    return os.listdir(path)

def get_immediate_subdirectories(a_dir):
    return [name for name in os.listdir(a_dir) if os.path.isdir(os.path.join(a_dir, name))]

def prepare_input_files(path):
    #assumption is the the path contains one folder for each program
    #we will open the folder and run the preprocessing method on it
    progList = get_immediate_subdirectories(path)
    for prog in progList:
        prog_path = os.path.join(path, prog)
        files = get_list_of_files(prog_path)
        if sum([1 for f in files if ".pyout" in f]) != 1:
            print "Exactly one .pyout file should be present in", prog_path
            return
        fname = [f for f in files if ".pyout" in f][0]
        full_fname = os.path.join(prog_path, fname)
        out1_name = os.path.join(prog_path, prog + ".in")
        print out1_name
        out1_file = open(out1_name, 'w')
        sp.call(["python", "pymodules/pre_process.py", full_fname, "preprocess"], stdout=out1_file)
        out1_file.close()

        out2_name = os.path.join(prog_path, prog + ".rr")
        print out2_name
        out2_file = open(out2_name, 'w')
        sp.call(["python", "pymodules/pre_process.py", full_fname, "transitive"], stdout=out2_file)
        out2_file.close() 
   
        out3_name = os.path.join(prog_path, prog + ".des")
        print out3_name
        out3_file = open(out3_name, 'w')
        sp.call(["python", "pymodules/pre_process.py", full_fname, "dependency"], stdout=out3_file)
        out3_file.close() 

        out4_name = os.path.join(prog_path, prog + ".pg")
        print out4_name
        out4_file = open(out4_name, 'w')
        sp.call(["python", "pymodules/pre_process.py", full_fname, "program_graph"], stdout=out4_file)
        out4_file.close() 


if __name__=="__main__":
    if len(sys.argv[1]) != 2:
        print "pass directory which has directories containing pyout files"
    prepare_input_files(sys.argv[1])

