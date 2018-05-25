import sys
import subprocess as sp
import os

def get_list_of_files(path):
    return os.listdir(path)

def get_immediate_subdirectories(a_dir):
    return [name for name in os.listdir(a_dir) if os.path.isdir(os.path.join(a_dir, name))]

def prepare_run_script(path, run_script_name):
    #take each .in file, assume there is a .rr file, run script should output back to this directory
    outf = open(run_script_name, 'w')
    outf.write("#!/bin/bash\n")
    progList = get_immediate_subdirectories(path)
   
    for prog in progList:
        prog_path = os.path.join(path, prog)
        
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
       
        m1 = str(m1)
        m2 = str(m2)
        t2 = str(t2)

        runcmd = "./opti"
        runcmd += " " + os.path.join(prog_path, prog)
        runcmd += " " + m1
        runcmd += " " + m2 
        runcmd += " 0"
        runcmd += " 1"
        runcmd += " " + t2
        runcmd += " > " + os.path.join(prog_path, prog) + "_0_1.log"
        runcmd += "\n"

        outf.write(runcmd)

if __name__=="__main__":
    if len(sys.argv) != 4:
        print "help: pass directory which has directories containing .in and .rr files"
    prepare_run_script(sys.argv[1], sys.argv[2])

