#!/bin/sh

export HOME=/home/steam
DBUS_SESSION_BUS_ADDRESS=$(cat /tmp/DBUS_SESSION_BUS_ADDRESS) /usr/bin/pulseaudio &
