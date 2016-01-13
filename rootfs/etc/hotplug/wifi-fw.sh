#!/bin/sh

# Sample environment dump when hotplug is called at the time of request_firmware
# ACTION=add
# HOME=/
# SEQNUM=441
# FIRMWARE=mrvl/helper_sd.bin
# DEVPATH=/devices/platform/mvsdio/mmc_host/mmc0/mmc0:0001/mmc0:0001:1/firmware/mmc0:0001:1
# SUBSYSTEM=firmware
# PATH=/sbin:/bin:/usr/sbin:/usr/bin
# TIMEOUT=60
# PWD=/

#echo "starting ..." >> /tmp/hotplug.$$
#echo $ACTION >> /tmp/hotplug.$$
#echo $SUBSYSTEM >> /tmp/hotplug.$$
#echo $DEVPATH >> /tmp/hotplug.$$
#echo $FIRMWARE >> /tmp/hotplug.$$
#echo $TIMEOUT >> /tmp/hotplug.$$
#echo $ASYNC >> /tmp/hotplug.$$
#echo "end" >> /tmp/hotplug.$$

#if [ $ACTION == "add" ] && [ $SUBSYSTEM == "firmware" ]; then

case $ACTION in
    add)
        #echo "ADD" >> /tmp/hotplug.$$
        case $SUBSYSTEM in
	    firmware)
		#echo "GOOD" >> /tmp/hotplug.$$
		#cat /sys/$DEVPATH/loading >> /tmp/hotplug.$$
		#ls -l /sys/devices/platform/mv_sdhci.0/mmc_host/mmc0/mmc0:0001/mmc0:0001:1 >> /tmp/hotplug.$$
		#ls -l /sys/devices/platform/mv_sdhci.0/mmc_host/mmc0/mmc0:0001/mmc0:0001:1/firmware >> /tmp/hotplug.$$
		#echo "CLASS" >> /tmp/hotplug.$$
		#ls -l /sys/class/firmware/ >> /tmp/hotplug.$$
		echo 1 > /sys/$DEVPATH/loading
		cat /lib/firmware/$FIRMWARE > /sys/$DEVPATH/data
		echo 0 > /sys/$DEVPATH/loading
		;;
	    *)
		#*echo "sub else" >> /tmp/hotplug.$$
	    ;;
	esac
        ;;
    *)
		#*echo "act else" >> /tmp/hotplug.$$
    ;;
esac

