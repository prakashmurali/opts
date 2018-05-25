#!/bin/bash
for i in `find ./$2 -name $1 -type f`;
do
  grep -l 'Solve Time' $i
  grep 'Solve Time' $i|tail -1
done
