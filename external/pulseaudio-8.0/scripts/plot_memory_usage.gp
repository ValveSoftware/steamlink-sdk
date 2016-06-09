
# This file is part of PulseAudio.
#
# Copyright 2015 Ahmed S. Darwish <darwish.07@gmail.com>
#
# PulseAudio is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# PulseAudio is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.

#
# PulseAudio memory usage plotting script
#
# Before invocation, generate necessary data by running the
# 'benchmark_memory_usage.sh' bash script. Afterwards, you can plot
# such data by running:
#
#       gnuplot plot_memory_usage.gp
#
# Note! To avoid scaling issues, memory is plotted in a "double y axis"
# form, with VM usage on the left, and dirty RSS memory usage on the
# right. The scales are different.
#

# Print our user messages to the stdout
set print "-"

benchDir = system('dirname ' .ARG0) .'/benchmarks/'
inputFile = benchDir ."memory-usage-LATEST.txt"
outputFile = benchDir ."pulse-memory-usage.png"

set title "PulseAudio Memory Usage Over Time"
set xlabel "Number of councurrent 'paplay' clients"

set ylabel "Virtual memory consumption (GiB)"
set y2label "Dirty RSS consumption (MiB)"
set ytics nomirror
set y2tics

# Finer granulrity for x-axis ticks ...
set xtics 1,1
set grid

# Use Cairo's PNG backend. This produce images which are way
# better-rendered than the barebone classical png backend
set terminal pngcairo enhanced size 1000,768 font 'Verdana,10'
set output outputFile

print "Plotting data from input file: ", inputFile
print "..."

plot inputFile using 1:($2/1024/1024) title "VmSize" axes x1y1 with linespoints, \
     inputFile using 1:($3/1024) title "Dirty RSS" axes x1y2 with linespoints

print "Done! Check our performance at: ", outputFile
