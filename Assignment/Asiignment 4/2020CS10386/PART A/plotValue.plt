set terminal png
set term png size 1000,400
set output "congestionRenoPlus3.png"
set title "Congestionwindow size vs. Time "
set xlabel "Time (Seconds)"
set ylabel "Congestion window"
plot "seventh3.cwnd"  with line
