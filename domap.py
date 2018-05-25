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

        sprog = prog.split("_")[1]

        baset2 = 1250
        if sprog == "8":
            m1 = 2
            m2 = 4
            t2 = baset2/2
        if sprog == "16":
            m1 = 2
            m2 = 8
            t2 = baset2
        if sprog == "32":
            m1 = 8
            m2 = 4
            t2 = baset2*2
        if sprog == "64":
            m1 = 8
            m2 = 8
            t2= baset2*4
        if sprog == "128":
            m1 = 16
            m2 = 8
            t2 = baset2*8
        if sprog == "256":
            m1 = 16
            m2 = 16
            t2 = baset2*16
        if sprog == "512":
            m1 = 32
            m2 = 16
            t2 = baset2*32
        

        out1_name = os.path.join(prog_path, prog + ".gmap")
        print full_fname
        out1_file = open(out1_name, 'w')
        sp.call(["time", "python", "pymodules/mapper.py", full_fname, str(m1), str(m2)], stdout=out1_file)
        out1_file.close()

if __name__=="__main__":
    if len(sys.argv[1]) != 2:
        print "pass directory which has directories containing pyout files"
    prepare_input_files(sys.argv[1])

