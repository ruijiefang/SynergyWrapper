# Scale font and line width (dpi) by changing the size! It will always display stretched.
set terminal svg size 400,300 enhanced fname 'arial'  fsize 10 butt solid
set output 'out.svg'

# Key means label...
set key inside top right
set xlabel 'G (chunks)'
set ylabel 'Time (s)'
set title '9000x9000 Matrix multiplication, 12 nodes x24 processes'
plot  "data.txt" using 1:2 title 'SynergyWrapper' with linespoints, "data.txt" using 1:3 title 'SynergyWrapper (with Ring)' with linespoints, "data.txt" using 1:4 title 'MPI 12x24' with linespoints

