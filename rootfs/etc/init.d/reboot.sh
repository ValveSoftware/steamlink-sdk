#!/bin/sh

# Ensure the network is taken down prior to reboot.  If the network is
# not taken down, there are occasional failures to reconnect to some
# wireless APs after reboot.  The Belkin FD8235-4v2 is one such AP
killall wpa_supplicant.sh &
killall wpa_supplicant &
sleep 2

reboot -f

