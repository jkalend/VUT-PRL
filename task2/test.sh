#!/bin/bash

if [ $# -lt 2 ];then
    echo "Usage: $0 <file with definiton of the playfield> <number of iterations>"
else
    file=$1;
    iter=$2;
fi;

mpic++ --prefix /usr/local/share/OpenMPI -o life life.cpp

procs=6

mpirun --prefix /usr/local/share/OpenMPI -np $procs life $file $iter

#uklid
rm -f life
