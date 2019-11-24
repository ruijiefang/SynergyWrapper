# Scale font and line width (dpi) by changing the size! It will always display stretched.
set terminal svg size 400,300 enhanced fname 'arial'  fsize 10 butt solid
set output 'out.svg'

# Key means label...
set key inside top right
set xlabel '# Processors (Node: 2xMPI-Groups for SNG)'
set ylabel 'Time (s)'
set title 'SynergyWrapper vs MPI (Xeon E5-1607, 4 Physical Cores)'
plot  "data.txt" using 1:2 title 'MPI' with linespoints, "data.txt" using 1:3 title 'Synergy' with linespoints

