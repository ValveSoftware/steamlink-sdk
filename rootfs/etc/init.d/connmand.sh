#!/bin/ash

#DEBUG_OPTIONS="-d"

if [[ -e /mnt/config/system/forget-networks.txt ]] || [[ -e /mnt/disk/steamlink/config/system/forget-networks.txt ]]; then
	rm -rf /var/lib/connman/wifi_*
	rm -f /mnt/config/system/forget-networks.txt
fi

while [ 1 = 1 ]; do
	# TODO: busybox supposedly supports pgrep -x, but in practice it doesn't
	# work.  It never finds the process
	running=$(pgrep -l connmand | grep -v connmand.sh | grep -c connmand)
	if [ "${running}" = "0" ]; then
		echo STARTING connmand 
		connmand --wifi=nl80211 ${DEBUG_OPTIONS}
	fi

	sleep 5
done

