# Scale font and line width (dpi) by changing the size! It will always display stretched.
set terminal svg size 400,300 enhanced fname 'arial'  fsize 10 butt solid
set output 'out.svg'

# Key means label...
set key inside top left
set xlabel '# Wrapper processes'
set ylabel 'Time (s)'
set title 'Single Host Tuple Space Overhead'
plot  "data.txt" using 1:2 title "tot=4" with linespoints, "data.txt" using 1:3 title "tot=8" with linespoints, "data.txt" using 1:4 title "tot=16" with linespoints, "data.txt" using 1:5 title "tot=32" with linespoints, "data.txt" using 1:6 title "tot=64" with linespoints
