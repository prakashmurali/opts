import sys
import subprocess as sp
import os

def get_list_of_files(path):
    return os.listdir(path)

def get_immediate_subdirectories(a_dir):
    return [name for name in os.listdir(a_dir) if os.path.isdir(os.path.join(a_dir, name))]

def write_cmd_in_slurm(prog, cmd):
    f = open(prog + '.slurm', 'w')
    f.write("#!/bin/bash\n")
    f.write("#SBATCH --nodes=1\n")
    f.write("#SBATCH --ntasks-per-node=1\n")
    f.write("#SBATCH --time=23:30:00\n")
    f.write("#SBATCH --mail-type=begin\n")
    f.write("#SBATCH --mail-type=end\n")
    f.write("#SBATCH --mail-user=pmurali@princeton.edu\n")
    f.write("cd /home/pmurali/code/opts/\n")
    f.write(cmd + '\n')
    f.close()


def prepare_run_script(path, run_script_name):
    #take each .in file, assume there is a .rr file, run script should output back to this directory
    outf = open(run_script_name, 'w')
    outf.write("#!/bin/bash\n")
    progList = get_immediate_subdirectories(path)
    #Note: This number is not essential, its just a placeholder, can be removed
    thresholdMap = {
        "BV2_1":50, 
        "BV4_1":50, 
        "BV4_3":80, 
        "BV6_1":50, 
        "BV6_3":150, 
        "BV6_5":275, 
        "BV8_1":50, 
        "BV8_3":100, 
        "BV8_5":250, 
        "BV8_7":300,
        "LPN4_2":80,
        "LPN4_3":100,
        "LPN6_2":80,
        "LPN6_3":100,
        "LPN6_4":150,
        "LPN6_5":200,
        "LPN8_2":80,  
        "LPN8_3":150,
        "LPN8_4":200,
        "LPN8_5":250,
    }
    
    processor = {
        "32": " 4 8 ",
        "64": " 8 8 ",
        "128": " 16 8 ",
        "256": " 32 8 ",
        "512": " 64 8 ",
    }

    for prog in progList:
        if prog in thresholdMap:
            threshold = thresholdMap[prog]
        else:
            threshold = 10000
        prog_path = os.path.join(path, prog)
        files = get_list_of_files(prog_path)
        if sum([1 for f in files if ".in" in f]) != 1:
            print "Exactly one .in file should be present in", prog_path
            return
        if sum([1 for f in files if ".rr" in f]) != 1:
            print "Exactly one .rr file should be present in", prog_path
            return
        fname_in = [f for f in files if ".in" in f][0]
        fname_rr = [f for f in files if ".rr" in f][0]
        fullfname_in = os.path.join(prog_path, fname_in)
        fullfname_rr = os.path.join(prog_path, fname_rr)
       
        if "BP" in prog:
            reqmachine = processor[prog.split('_')[1]]

        out1 = os.path.join(prog_path, prog + '_ND.out')
        log1 = os.path.join(prog_path, prog + '_ND.log')
        cmd1 = "./opti " + fullfname_in + " " + fullfname_rr + " " + out1 + " 0 2 " + str(threshold) +  reqmachine + " > " + log1 + "\n"
        print 'Processing', prog
        write_cmd_in_slurm(prog, cmd1)

if __name__=="__main__":
    if len(sys.argv) != 4:
        print "help: pass directory which has directories containing .in and .rr files"
    prepare_run_script(sys.argv[1], sys.argv[2])

