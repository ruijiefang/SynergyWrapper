#

for n_hosts in `seq 3 12`;do
	for n_procs in `seq 12 24`;do
		./matrix_master.sh $n_hosts $n_procs
	done
done
