#!/bin/bash
python post_process.py ../input/$1.pyout ../logs/$1.d_1b.out onebend data > ../logs/$1.d_1b.qasm
#python post_process.py ../input/$1.pyout ../logs/$1.nd_1b.out onebend nodata > ../logs/$1.d_1b.qasm

