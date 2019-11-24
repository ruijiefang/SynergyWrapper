# Scale font and line width (dpi) by changing the size! It will always display stretched.
set terminal svg size 400,300 enhanced fname 'arial'  fsize 10 butt solid
set output 'out.svg'

# Key means label...
set key inside top right
set xlabel 'G (rows)'
set ylabel 'Time (s)'
set title '2000x2000, 4x8 Granularity Tuning'
plot  "data.txt" using 1:2 title '4 MPI-Groups x 8 MPI threads' with lines, "data.txt" using 1:3 title "MPI x32" with lines

