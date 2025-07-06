#!/bin/sh

SMALL_DIR=/tmp/fra/small
BIG_DIR=/tmp/fra/big

./generate_file_dat.sh

./minizseq -r 1 $SMALL_DIR >> small_files_seq
./minizseq -r 1 $BIG_DIR >> big_files_seq

for j in 4 8 16 32; do
  echo "$j threads"
  for i in $(seq 1 5); do
    OMP_NUM_THREADS=$j ./minizparallel -r 1 $SMALL_DIR >> small_files
  done
done

rm -rf /tmp/fra/small

for j in 4 8 16 32; do
  echo "$j threads"
  for i in $(seq 1 5); do
    OMP_NUM_THREADS=$j ./minizparallel -r 1 $BIG_DIR >> big_files
  done
done

rm -rf /tmp/fra

