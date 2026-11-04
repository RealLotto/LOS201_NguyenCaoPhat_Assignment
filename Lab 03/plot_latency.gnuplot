set terminal pngcairo size 1600,1000 noenhanced font "DejaVu Sans,11"
set output 'logs/latency_histogram.png'

stats 'logs/hist_standard.txt' using 1:($2+$3+$4+$5) name 'standard' nooutput
standard_samples = standard_sum_y
standard_max = standard_max_x
stats 'logs/hist_standard.txt' using 1:($1*($2+$3+$4+$5)) name 'standard_weighted' nooutput
standard_mean = standard_weighted_sum_y / standard_samples

stats 'logs/hist_realtime.txt' using 1:($2+$3+$4+$5) name 'realtime' nooutput
realtime_samples = realtime_sum_y
realtime_max = realtime_max_x
stats 'logs/hist_realtime.txt' using 1:($1*($2+$3+$4+$5)) name 'realtime_weighted' nooutput
realtime_mean = realtime_weighted_sum_y / realtime_samples
max_reduction = 100.0 * (1.0 - realtime_max / standard_max)

set multiplot layout 2,1 title sprintf( \
    'Cyclictest: kernel thường vs PREEMPT_RT | TB thường/RT %.2f/%.2f µs | Max thường/RT %.0f/%.0f µs (giảm %.1f%%)', \
    standard_mean, realtime_mean, standard_max, realtime_max, max_reduction) font ",15"
set logscale y 10
set grid xtics ytics back lc rgb "#d8dee8"
set ylabel 'Số mẫu (%)'
set format y '%.3g'
set key top right opaque box
set style data steps
set style line 1 lc rgb "#0072B2" lw 2.5
set style line 2 lc rgb "#D55E00" lw 2.5

set title 'Vùng độ trễ phổ biến (0–40 µs)'
set xlabel 'Độ trễ (µs)'
set xrange [0:40]
set xtics 5
set yrange [0.001:100]
plot 'logs/hist_standard.txt' using 1:(100.0*($2+$3+$4+$5)/standard_samples) \
         ls 1 title sprintf('Kernel thường (max %.0f µs)', standard_max), \
     'logs/hist_realtime.txt' using 1:(100.0*($2+$3+$4+$5)/realtime_samples) \
         ls 2 title sprintf('PREEMPT_RT (max %.0f µs)', realtime_max)

set title 'Đuôi phân bố và các đợt trễ hiếm (10–200 µs)'
set xlabel 'Độ trễ (µs)'
set xrange [10:200]
set xtics 20
set yrange [0.001:1]
set label 1 sprintf('Max: %.0f µs (thường) so với %.0f µs (PREEMPT_RT)', \
    standard_max, realtime_max) at graph 0.03,0.88 front
plot 'logs/hist_standard.txt' using 1:(100.0*($2+$3+$4+$5)/standard_samples) \
         ls 1 title sprintf('Kernel thường (max %.0f µs)', standard_max), \
     'logs/hist_realtime.txt' using 1:(100.0*($2+$3+$4+$5)/realtime_samples) \
         ls 2 title sprintf('PREEMPT_RT (max %.0f µs)', realtime_max)

unset multiplot
