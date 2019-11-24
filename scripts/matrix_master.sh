#!/bin/bash

#
# Matrix master batch script
# by Ruijie Fang
#

# Usage: For each matrix size supplied in `master.matrix` file,
# Create a `s<N>.g` granularity tuning file that contains all the
# 'G' points you want to tune. Values shall be (0,N/P].
# Group size & nodes per group need to be passed in as arg1 and arg2.

if [ $1 == 'usage' ];then
	echo "Usage: ./matrix_master.sh <group total> <group size>"
	echo "Input files: 1) master.matrix; 2) multiple s<N>.g files"
	echo
fi

group_total=$1
group_size=$2

curr=`pwd`
for m_size in `cat master.matrix`; do
	for m_chunk in `cat "s"$m_size".g"`;do
		echo " *** Testing Matrix @"$m_size" "$m_chunk" ***"
		$curr/launch_all_tsd.sh $group_total > "tsdLaunch_Group"$1"-Size"$2"-M_Size"$m_size"-G"$m_chunk".log"
		echo "   * -----> Success starting tsd <------- *"
		$curr/matrix.sh $group_total $group_size $m_size $m_chunk > "Group"$1"-Size"$2"-M_Size"$m_size"-G"$m_chunk".log"
		echo "   * -----> Successful first run <-------- *"
		$curr/matrix.sh $group_total $group_size $m_size $m_chunk >> "Group"$1"-Size"$2"-M_Size"$m_size"-G"$m_chunk".log"
#		$curr/matrix.sh $group_total $group_size $m_size $m_chunk >> "Group"$1"-Size"$2"-M_Size"$m_size"-G"$m_chunk".log"
		echo " *** Finishing Calculations *** "
		echo
		$curr/kill_all_tsd.sh 1 $group_total
	done
done
