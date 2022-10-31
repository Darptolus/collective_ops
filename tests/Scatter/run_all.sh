#!/usr/bin/env bash

DIR_NAME="results_s22_2"
mkdir $DIR_NAME

# 256; // 1 Kb
# 2560; // 10 Kb
# 25600; // 100 Kb
# 262144; // 1 Mb
# 2621440; // 10 Mb
# 26214400; // 100 Mb
# 209715200; // 800 Mb
# 235929600; // 900 Mb
# 268435456; // 1 Gb
# 1342177280; // 5 Gb

for i in 256 2560 25600 262144 2621440 26214400 209715200 235929600
# for i in 235929600
do 
  export CHK_SZ=$i
  for j in $(seq 0 11)
  do
    # echo "ARR_SZ = $ARR_SZ"
    ./scatter_22.x>>./$DIR_NAME/out_test_$i.o  2>&1; wait
  done
done

# export ARR_SZ=100; ./broadcast_20.x
# for i in 209715200
# for i in 2684354560
# for i in 1342177280
# for i in 10