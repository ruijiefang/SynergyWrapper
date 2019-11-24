#!/bin/bash

rm ./tsd1.log
rm ./tsd2.log
killall -9 sng_tsd
bash ./cleanup.sh
../cmake-build-debug/sng_tsd ../etc/synergy-config-1.json ../etc/synergy-log.conf >> ./tsd1.log &
../cmake-build-debug/sng_tsd ../etc/synergy-config-2.json ../etc/synergy-log.conf >> ./tsd2.log &

echo " ====== tsd launch complete. You have 2 tsd instances. ====== "
ps aux | grep sng_tsd
echo " ============================================================ "
