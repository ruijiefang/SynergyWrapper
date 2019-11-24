#!/bin/bash

rm ./tsd1.log
rm ./tsd2.log
killall -9 sng_tsd
bash /home/cc/Synergy4/s4c00/scripts/cleanup.sh

echo " *============== Launching remote tsd at "$1" =============* "

/home/cc/Synergy4/s4c00/cmake-build-debug/sng_tsd /home/cc/Synergy4/s4c00/etc/sng$1.json /home/cc/Synergy4/s4c00/etc/synergy-log.conf >> /home/cc/logs/tsd$1.log &

echo " ====== tsd launch complete. You have 1 tsd instance. ====== "
ps aux | grep sng_tsd
echo " ============================================================ "
