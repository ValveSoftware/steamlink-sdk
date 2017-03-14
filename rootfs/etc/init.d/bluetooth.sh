#!/bin/sh

#DEBUG_OPTIONS=-d

# Set SCO into HCI mode
hcitool cmd 0x3f 0x1d 0x00

while true; do
	echo "Starting bluetoothd: $DEBUG_OPTIONS"
	/usr/libexec/bluetooth/bluetoothd -E -n $DEBUG_OPTIONS
	sleep 1
done
