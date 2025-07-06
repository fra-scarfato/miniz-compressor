#!/bin/bash

# === Configuration ===
DIR="/tmp/fra"
NUM_SMALL=200         # Number of small files
NUM_BIG=5             # Number of big files

# === Size Ranges ===
SMALL_SIZE=500         # in KB

BIG_SIZE=51200        # in KB (50 MB)

# === Prepare directory ===
mkdir -p "$DIR/small"
mkdir -p "$DIR/big"


# === Generate small files ===
for i in $(seq 1 $NUM_SMALL); do
    fname="small_$i.dat"
    dd if=/dev/urandom of="$DIR/small/$fname" bs=1K count=$SMALL_SIZE status=none
done

# === Generate big files ===
for i in $(seq 1 $NUM_BIG); do
    fname="big_$i.dat"
    dd if=/dev/urandom of="$DIR/big/$fname" bs=1K count=$BIG_SIZE status=none
done

