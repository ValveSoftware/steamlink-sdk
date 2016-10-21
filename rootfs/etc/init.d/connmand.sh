#!/bin/ash

#DEBUG_OPTIONS="-d"

if [[ -e /mnt/config/system/forget-networks.txt ]] || [[ -e /mnt/disk/steamlink/config/system/forget-networks.txt ]]; then
	rm -rf /var/lib/connman/wifi_*
	rm -f /mnt/config/system/forget-networks.txt
fi

while true; do
	echo STARTING connmand 
	connmand -n --wifi=nl80211 ${DEBUG_OPTIONS}
done

