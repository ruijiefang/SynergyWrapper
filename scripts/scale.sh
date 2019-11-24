#
# scale.sh scales the matrix tests

for x in `seq 3 12`;do
	for y in `seq 4 24`;do
		echo " ** scale.sh: doing Group="$x", Procs="$y" **"
		./matrix_master.sh $x $y
	done
done
