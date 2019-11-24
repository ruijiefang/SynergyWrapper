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

group_size=18
curr=`pwd`
m_chunk=0
for group_total in `seq 2 12`;do
	for m_size in `cat master.matrix`; do
		let m_chunk=m_size/group_total	
		echo " *** Testing Matrix @"$m_size" Group="$group_total" "$m_chunk" ***"
		$curr/launch_all_tsd.sh $group_total > "tsdLaunch_Scale_Group_"$group_total"_Worker"$group_size"_Size"$m_size"_Chunk"$m_chunk".log"
		echo "   * -----> Success starting tsd <------- *"
		$curr/matrix.sh $group_total $group_size $m_size $m_chunk > "Scale_Group_"$group_total"_Worker"$group_size"_Size"$m_size"_Chunk"$m_chunk".log"
		echo "   * -----> Successful first run <-------- *"
		$curr/matrix.sh $group_total $group_size $m_size $m_chunk >> "Scale_Group_"$group_total"_Worker"$group_size"_Size"$m_size"_Chunk"$m_chunk".log"
#		$curr/matrix.sh $group_total $group_size $m_size $m_chunk >> "Group"$1"-Size"$2"-M_Size"$m_size"-G"$m_chunk".log"
		echo " *** Finishing Calculations *** "
		echo
		$curr/kill_all_tsd.sh 1 $group_total
	done
done
