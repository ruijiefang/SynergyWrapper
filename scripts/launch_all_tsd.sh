#!/bin/bash
# Launches all remote tsds at given hosts
# @author Ruijie Fang, Mar 2018

for x in `seq 1 $1`; do
	ssh sng-$x /home/cc/Synergy4/s4c00/scripts/launch_tsd.sh $x
done

