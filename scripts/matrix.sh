#!/bin/bash
# this script launches matrix multiplication sample
# @author Ruijie Fang, Mar 2018
# XXX: assumption: for multi-node tests, assumes tsd is already running on each node
# with stable ring configuration
# TODO: Change from generating hostnames via 'seq' to actually traversing a hostname file
# TODO 2: And separate script from multi-node tests and single-node test
if [ $1 == 'usage' ]; then
    echo "Usage: ./matrix.sh <MPI Group Count> <Threads per Group> <Matrix Size> <G>"
    exit 1
fi
if [ $1 == 'Usage' ]; then
    echo "Usage: ./matrix.sh <MPI Group Count> <Threads per Group> <Matrix Size> <G>"
    exit 1
fi

echo " ===== launching single host matrix multiplication program ===== "
echo " # MPI Groups:"$1
echo " # Threads per group:"$2
echo " # SIZE:"$3
echo " # G:   "$4
echo " ###########################"
# Maintainer's note: Sleeps are holy, please do not touch them!
rm ./matrix_worker_*.log
# XXX: Iterate thru each hostname. For now, this suffices
for x in `seq 2 $1`;do
# XXX: un-comment/exchange the line below if running on single host with two tsds
#	mpirun -np $2 ../cmake-build-debug/matrix_worker "ipc:///tmp/tsd2" $3 $4 > matrix_worker_$x.log &
	echo " ** Launching SynergyWrapper process@"$x"..."
	ssh sng-$x mpirun -np $2 /home/cc/Synergy4/s4c00/cmake-build-debug/matrix_worker "ipc:///tmp/tsd1" $3 $4 > /home/cc/logs/matrix_worker_sng$x.log &
        sleep 1 # XXX: Sleep 1sec to ensure load balancing
done

sleep 5
mpirun -np $2 ../cmake-build-debug/matrix_master "ipc:///tmp/tsd1" $3 $4

sleep 1

echo "terminating matrix workers..."
for x in `seq 2 $1`;do
	ssh sng-$x killall -9 matrix_worker
	ssh sng-$x killall -9 mpirun
done

echo " =========== SUCCESS ============="
