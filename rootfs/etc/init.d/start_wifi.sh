#!/bin/ash

SERIAL_FILE=/mnt/factory_setting/serial
BOARD_SPECIFIC_CAL_FILE=mrvl/WlanCalData.conf
DEFAULT_CAL_FILE=mrvl/WlanCalData_ext_Golden.conf
EV10_CAL_FILE=mrvl/WlanCalData_ev10.conf
EV11_CAL_FILE=mrvl/WlanCalData_ev11.conf

INITCFG_FILE=mrvl/WlanInitCfgData.conf
MACADDR_FILE=mrvl/WlanMacAddr.conf

FACTORY_DIR=/mnt/factory_setting
WIFI_MAC_FILE=${FACTORY_DIR}/WIFI_MAC_ADDR

TXPWRLIMIT_FILE=mrvl/txpwrlimit_cfg.bin

ED_MAC_CTRL_FILE=mrvl/ed_mac_ctrl.conf

# helper functions
is_ev10_board() {
	is_ev1x=$(grep '^EV1' ${SERIAL_FILE})
	if [ -n "${is_ev1x}" ]; then
		# Is EV1xxxx
		serial=$(cat ${SERIAL_FILE} | sed 's/^EV1//')
		if [ $serial -le 150 ]; then
			echo "1"
		else
			echo "0"
		fi
	else
		echo "0"
	fi
}

is_ev11_board() {
	is_ev1x=$(grep '^EV1' ${SERIAL_FILE})
	if [ -n "${is_ev1x}" ]; then
		# Is EV1xxxx
		serial=$(cat ${SERIAL_FILE} | sed 's/^EV1//')
		if [ $serial -gt 150 ]; then
			echo "1"
		else
			echo "0"
		fi
	else
		echo "0"
	fi
}

get_group_cal_file() {
	for f in `ls /lib/firmware/mrvl/groups/group_*`; do
		serial=$(cat ${SERIAL_FILE})
		if [ -n "$(grep ${serial} ${f})" ]; then
			echo "mrvl/groups/$(basename ${f})_caldata"
		fi
	done
}

get_wifi_mac_address() {
	# Get mac address from file and insert semicolons between octets:
	# 021122334455 => 02:11:22:33:44:55
	cat $WIFI_MAC_FILE | sed -r 's/(..)(..)(..)(..)(..)(..)/\1:\2:\3:\4:\5:\6/'
}

# wifi
device=$(cat /sys/class/mmc_host/mmc0/mmc0:0001/mmc0:0001:1/device)
if [ "$device" = "0x9119" ]; then
	# 8787
	echo "modprobe 87mlan"
	modprobe 87mlan 2>/dev/null
	echo "modprobe sd8787"
	modprobe sd8787 2>/dev/null

	# Use old way of setting random WiFi MAC address
	if [ -f "${FACTORY_DIR}/WIFI_MAC_ADDR" ]; then
		# use factory setting if available
		mac_addr=$(cat $FACTORY_DIR/WIFI_MAC_ADDR)
		busybox ifconfig mlan0 hw ether $mac_addr
	else
		# otherwise generate a random MAC address
		mac_addr=`( busybox dd if=/dev/urandom bs=3 count=1 2> /dev/null) | busybox hexdump -e '3/1 "%02X"'`
		mac_addr=005043$mac_addr
		busybox ifconfig mlan0 hw ether $mac_addr

		# store MAC address to factory setting
		busybox mount -orw,remount $FACTORY_DIR
		echo $mac_addr > $FACTORY_DIR/WIFI_MAC_ADDR
		busybox mount -oro,remount $FACTORY_DIR
	fi
else
	# 8897
	# Set interface mode for nl80211 sta
	MOD_ARG="cfg80211_wext=0x4"
	# Read mac address from file if it exists
	if [ -f "${WIFI_MAC_FILE}" ]; then
		MOD_ARG="${MOD_ARG} mac_addr=$(get_wifi_mac_address)"
	else
		echo "warning wifi mac address file not found: ${WIFI_MAC_FILE}"
	fi

	# Select calibration file
	# 1. If this board has an individual cal file in factory data, it is used
	# 2. [Option: If serial# matches group X, load golden cal file X].
	#    Group files are in the form: /lib/firmware/mrvl/groups/<groupname>
	#    Corresponding caldata is:    /lib/firmware/mrvl/groups/<groupname>_caldata
	# 3. If this is an EV1.0 board (serial EV1000 - EV1150), use EV1.0 golden cal file from filesystem
	# 4. If this is an EV1.1 board (serial EV1151->), use EV1.1 golden cal file from filesystem
	# 5. Default Cal file.  NOTE: This is a fallback meant only for 
	#    development units that have no other calibration file.  All
	#    consumer boards should have individualized calibration.
	# 6. No cal
	if [ -s "/lib/firmware/${BOARD_SPECIFIC_CAL_FILE}" ]; then
		echo "Board cal file found!"
		MOD_ARG="${MOD_ARG} cal_data_cfg=${BOARD_SPECIFIC_CAL_FILE}"
	elif [ -n "$(get_group_cal_file)" ]; then
		MOD_ARG="${MOD_ARG} cal_data_cfg=$(get_group_cal_file)"
	elif [ $(is_ev10_board) -eq 1 ]; then
		if [ -s "/lib/firmware/${EV10_CAL_FILE}" ]; then
			MOD_ARG="${MOD_ARG} cal_data_cfg=${EV10_CAL_FILE}"
		fi
	elif [ $(is_ev11_board) -eq 1 ]; then
		if [ -s "/lib/firmware/${EV11_CAL_FILE}" ]; then
			MOD_ARG="${MOD_ARG} cal_data_cfg=${EV11_CAL_FILE}"
		fi
	elif [ -s "/lib/firmware/${DEFAULT_CAL_FILE}" ]; then
		echo "Warning: falling back to default cal file!"
		MOD_ARG="${MOD_ARG} cal_data_cfg=${DEFAULT_CAL_FILE}"
	else
		echo "Error: cal file is missing!"
	fi

	if [ -s "/lib/firmware/${INITCFG_FILE}" ]; then
		MOD_ARG="${MOD_ARG} init_cfg=${INITCFG_FILE}"
	fi

	# Load tx power limit file (if present)
	if [ -s "/lib/firmware/${TXPWRLIMIT_FILE}" ]; then
		MOD_ARG="${MOD_ARG} txpwrlimit_cfg=${TXPWRLIMIT_FILE}"
	fi

        echo "modprobe 8897mlan"
        modprobe 8897mlan 2>/dev/null
        echo "modprobe sd8897 ${MOD_ARG}"
        modprobe sd8897 ${MOD_ARG} 2>/dev/null
	ifconfig mlan0 up
	
	echo "Applying WiFi ED MAC ctrl"
	/usr/sbin/mlanutl mlan0 hostcmd /lib/firmware/${ED_MAC_CTRL_FILE} ed_mac_ctrl_v2
	
	echo "Enabling WiFi debug dumps"
	/usr/sbin/mlanutl mlan0 drvdbg 0x80007
fi
