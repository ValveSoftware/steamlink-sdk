#!/bin/sh

#DEBUG_OPTIONS=-d

while true; do
	echo "Starting bluetoothd: $DEBUG_OPTIONS"
	/usr/libexec/bluetooth/bluetoothd -E -n $DEBUG_OPTIONS
	sleep 1
done
