#!/bin/sh

export HOME=/home/steam
dbus-daemon --fork --session --print-address > /tmp/DBUS_SESSION_BUS_ADDRESS
DBUS_SESSION_BUS_ADDRESS=$(cat /tmp/DBUS_SESSION_BUS_ADDRESS) /usr/bin/pulseaudio &
