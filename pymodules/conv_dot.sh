#!/bin/bash 
dot -Tpdf $1_final.dot -o $1_$2_final.pdf
dot -Tpdf $1_pp.dot -o $1_$2_pp.pdf
dot -Tpdf $1_preroute.dot -o $1_$2_preroute.pdf
dot -Tpdf $1_swaps.dot -o $1_$2_swaps.pdf
#okular $1.pdf &

