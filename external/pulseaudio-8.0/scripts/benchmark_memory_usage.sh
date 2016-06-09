#!/bin/bash

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
# Measure PulseAudio memory usage (VmSize, Private+Shared_Dirty)
# while increasing the number of connected clients over time.
#
# To avoid premature client exits, please ensure giving a long
# wave file as the script's first parameter.
#

_bold="\x1B[1m"
_error="\x1B[1;31m"
_reset="\x1B[0m"

BASENAME=$(basename $0)
PROMPT="${_bold}[$BASENAME]${_reset}"
error() {
    echo -e "$PROMPT: ** Error: ${_error}$1${_reset}" >&2; exit -1
}
msg() {
    echo -e "$PROMPT: $1"
}

_absolute_dirname="$(dirname `readlink -f $0`)"
PA_HOME="${_absolute_dirname%/scripts}"
[ -d "$PA_HOME/src" -a -d "$PA_HOME/scripts" ] ||
    error "This script can only be executed from PulseAudio source tree"

PA=${PA_HOME}/src/pulseaudio
PA_CAT=${PA_HOME}/src/pacat
PA_PLAY=${PA_HOME}/src/paplay
PA_FLAGS="-n -F ${PA_HOME}/src/default.pa -p ${PA_HOME}/src/"

[ -L "$PA_PLAY" ] || ln -sf .libs/lt-pacat $PA_PLAY
PA_PLAY_PROCESS_NAME="paplay"

SCRIPTS_DIR=${PA_HOME}/scripts
BENCHMARKS_DIR=${SCRIPTS_DIR}/benchmarks
GNUPLOT_SCRIPT=${SCRIPTS_DIR}/plot_memory_usage.gp
OUTPUT_FILE=${BENCHMARKS_DIR}/memory-usage-`date -Iseconds`.txt
SYMLINK_LATEST_OUTPUT_FILE=${BENCHMARKS_DIR}/memory-usage-LATEST.txt

MAX_CLIENTS=30

[ -e "$PA" ] || error "$PA does not exist. Compile PulseAudio tree first."
[ -x "$PA" ] || error "$PA cannot be executed"
[ -x "$PA_CAT" ] || error "$PA_CAT cannot be executed"

AUDIO_FILE="$1"
[ -n "$AUDIO_FILE" ] || error "Usage: $BASENAME AUDIO_FILE"
[ -e "$AUDIO_FILE" ] || error "$AUDIO_FILE does not exist"
[ -f "$AUDIO_FILE" ] || error "$AUDIO_FILE is not a regular file"

$PA --check >/dev/null 2>&1
[ "$?" -ne "0" ] || {
    msg "A PulseAudio daemon is already running on your system"
    msg "To use this script, please do the following first:"
    msg " 1. Add autospawn=no to $HOME/.pulse/client.conf"
    msg " 2. Kill current daemon instance using 'pulseaudio --kill'"
    error "Failed to start PulseAudio daemon"
}

msg "Hello. Benchmarking PulseAudio daemon memory usage over time"

# Check Linux Kernel's Documentation/sysctl/vm.txt for details.
msg "Ensuring consistent results by dropping all VM caches!"
sudo bash -c "sync && echo 3 >/proc/sys/vm/drop_caches"

msg "Starting PulseAudio daemon"
PULSE_LOG=0 PULSE_LOG_COLORS= PULSE_LOG_META= $PA $PA_FLAGS &
_pid=$!

# Give PA daemon time to initialize everything and vacuum. We want
# to make the _starting_ dirty RSS memory usage (0 clients) as
# equivalent as possible for multiple trials.
sleep 12

$PA --check >/dev/null 2>&1
[ "$?" -eq "0" ] || error "Failed to start PulseAudio daemon"

echo "# ClientCount    VmSize (KiB)     DirtyRSS (KiB)" >$OUTPUT_FILE

msg "Starting PulseAudio clients"
_i=1;
while true; do
    [ "$_i" -le "$MAX_CLIENTS" ] || break

    _vmsize=`ps -o vsize= $_pid`
    _drss=`awk '/(Shared|Private)_Dirty:/{ sum += $2 } END { print sum }' /proc/$_pid/smaps`
    [ "$?" -eq "0" ] || error "Error sampling PulseAudio RSS memory usage"

    echo "  $_i              $_vmsize           $_drss" >>$OUTPUT_FILE

    $PA_PLAY $AUDIO_FILE 2>/dev/null &
    _i=$((_i + 1))

    sleep 1
done
msg "Finished starting ${MAX_CLIENTS} PulseAudio clients over time"

_n_clients_still_alive=$(ps -C $PA_PLAY_PROCESS_NAME --no-headers | wc -l)
[ "$_n_clients_still_alive" -ge "$MAX_CLIENTS" ] || {
    msg "You did not provide a long-enough wave file for clients to play"
    msg "Only $_n_clients_still_alive clients are now active; expected $MAX_CLIENTS"
    error "Please provide a large wave file (~ $((MAX_CLIENTS*2))s) then redo the benchmarks"
}

msg "Killing PulseAudio daemon"
$PA --kill >/dev/null 2>&1

rm -f $SYMLINK_LATEST_OUTPUT_FILE
ln -s $OUTPUT_FILE $SYMLINK_LATEST_OUTPUT_FILE

msg "Sampling daemon memory usage done!"
msg "Check the results at $SYMLINK_LATEST_OUTPUT_FILE"
msg "Plot these results using 'gnuplot $GNUPLOT_SCRIPT'"
