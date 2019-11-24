
#!/bin/bash

# kills all tsd in given range

for x in `seq $1 $2`;do
	echo "killing tsd @ "$x"..."
	ssh sng-$x killall -9 sng_tsd
	ssh sng-$x killall -9 matrix_worker
	ssh sng-$x killall -9 matrix_master
	ssh sng-$x killall -9 mpirun
done
