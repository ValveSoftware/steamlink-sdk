#!/bin/sh

FACTORY_DIR=/mnt/factory_setting
BT_MAC_FILE=${FACTORY_DIR}/BT_MAC_ADDR
BT_CAL=mrvl/BTCalData_sd8897.conf

get_bt_mac_address() {
	# Get mac address from file and insert semicolons between octets:
	# 021122334455 => 02:11:22:33:44:55
	cat $BT_MAC_FILE | sed -r 's/(..)(..)(..)(..)(..)(..)/\1:\2:\3:\4:\5:\6/'
}


# Bluetooth
# note: the bluetooth module must be insmod'ed after wifi or else the
# chip firmware is not loaded properly and bluetooth will fail to work
if [ -f "${BT_MAC_FILE}" ]; then
	BG_MAC_ARG="bt_mac=$(get_bt_mac_address)"
else
	echo "warning bluetooth mac address file not found: ${BT_MAC_FILE}"
fi

echo "modprobe bt8xxx ${BG_MAC_ARG} cal_cfg=${BT_CAL} psmode=0"
modprobe bt8xxx ${BG_MAC_ARG} cal_cfg=${BT_CAL} psmode=0

/etc/init.d/bluetooth.sh &
