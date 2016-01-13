#!/bin/ash

services=$(connmanctl services | grep -e '^[*]' | grep -v Wired | sed -e 's/.* wifi/wifi/')

echo "Removing Favorite Networks ..."
sleep 2
if [ "$services" != "" ]; then
	for service in $services; do
		echo "Removing network $service ..."
		connmanctl config $service --remove
		# note: if the network fails to remove it is immutable,
		#       which probably means it is coming in from a
		# configuration file /var/lib/connman/*.conf
		#
		sleep 1
	done
fi

echo "Removing Immutable Services ..." 
rm -f /var/lib/connman/*.config

