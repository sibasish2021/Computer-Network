set terminal png
set term png size 1000,400
set output "congestion.png"
set title "Congestionwindow size vs. Time "
set xlabel "Time (Seconds)"
set ylabel "Congestion window"
plot "Q3.txt"  with line
