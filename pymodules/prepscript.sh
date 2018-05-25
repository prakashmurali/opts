#!/bin/bash
python pre_process.py ../input/$1.pyout preprocess > ../input/$1.in
python pre_process.py ../input/$1.pyout transitve > ../input/$1.rr

