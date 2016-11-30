#!/bin/bash

# default devices
dev_playback="default"
dev_capture="default"

bin="alsabat"
commands="$bin -P $dev_playback -C $dev_capture"

file_sin_mono="default_mono.wav"
file_sin_dual="default_dual.wav"
logdir="tmp"

# frequency range of signal
maxfreq=16547
minfreq=17

# features passes vs. features all
feature_pass=0
feature_cnt=0

init_counter () {
	feature_pass=0
	feature_all=0
}

evaluate_result () {
	feature_cnt=$((feature_cnt+1))
	if [ $1 -eq 0 ]; then
		feature_pass=$((feature_pass+1))
		echo "pass"
	else
		echo "fail"
	fi
}

print_result () {
	echo "[$feature_pass/$feature_cnt] features passes."
}

feature_test () {
	echo "============================================"
	echo "$feature_cnt: ALSA $2"
	echo "-------------------------------------------"
	echo "$commands $1 --log=$logdir/$feature_cnt.log"
	$commands $1 --log=$logdir/$feature_cnt.log
	evaluate_result $?
	echo "$commands $1" >> $logdir/$((feature_cnt-1)).log
}

# test items
feature_list_test () {
	init_counter

	commands="$bin"
	feature_test "-c1 --saveplay $file_sin_mono" \
			"generate mono wav file with default params"
	feature_test "-c2 --saveplay $file_sin_dual" \
			"generate dual wav file with default params"
	sleep 5
	feature_test "-P $dev_playback" "single line mode, playback"
	feature_test "-C $dev_capture --standalone" "single line mode, capture"

	commands="$bin -P $dev_playback -C $dev_capture"
	feature_test "--file $file_sin_mono" "play mono wav file and detect"
	feature_test "--file $file_sin_dual" "play dual wav file and detect"
	feature_test "-c1" "configurable channel number: 1"
	feature_test "-c2 -F $minfreq:$maxfreq" "configurable channel number: 2"
	feature_test "-r44100" "configurable sample rate: 44100"
	feature_test "-r48000" "configurable sample rate: 48000"
	feature_test "-n10000" "configurable duration: in samples"
	feature_test "-n2.5s" "configurable duration: in seconds"
	feature_test "-f U8" "configurable data format: U8"
	feature_test "-f S16_LE" "configurable data format: S16_LE"
	feature_test "-f S24_3LE" "configurable data format: S24_3LE"
	feature_test "-f S32_LE" "configurable data format: S32_LE"
	feature_test "-f cd" "configurable data format: cd"
	feature_test "-f dat" "configurable data format: dat"
	feature_test "-F $maxfreq --standalone" \
			"standalone mode: play and capture"
	latestfile=`ls -t1 /tmp/bat.wav.* | head -n 1`
	feature_test "--local -F $maxfreq --file $latestfile" \
			"local mode: analyze local file"
	feature_test "--roundtriplatency" \
			"round trip latency test"
	feature_test "--snr-db 26" \
			"noise detect threshold in SNR(dB)"
	feature_test "--snr-pc 5" \
			"noise detect threshold in noise percentage(%)"

	print_result
}

echo "*******************************************"
echo "                BAT Test                   "
echo "-------------------------------------------"

# get device
echo "usage:"
echo "  $0 <sound card>"
echo "  $0 <device-playback> <device-capture>"

if [ $# -eq 2 ]; then
	dev_playback=$1
	dev_capture=$2
elif [ $# -eq 1 ]; then
	dev_playback=$1
	dev_capture=$1
fi

echo "current setting:"
echo "  $0 $dev_playback $dev_capture"

# run
mkdir -p $logdir
feature_list_test

echo "*******************************************"
