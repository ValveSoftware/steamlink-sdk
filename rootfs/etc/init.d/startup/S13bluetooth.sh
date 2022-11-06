#!/bin/sh

FACTORY_DIR=/mnt/factory_setting
DEVICE_ID=${FACTORY_DIR}/device_id.txt
BT_CAL=mrvl/BTCalData_sd8897.conf
BT_OID=E0319E

get_bt_mac_address() {
	address=$(ls -d /var/lib/bluetooth/*/* 2>/dev/null | fgrep : | fgrep -v cache | fgrep -v settings | sed 's,/var/lib/bluetooth/\(.*\)/.*,\1,' | head -1)
	if [ "$address" = "" ]; then
		id=$(echo $(($(cat ${DEVICE_ID} | sed 's/.*1003_0*//'))))
		hi=$(($id & 0xffffc0))
		lo=$(($id & 0x00003f))
		address=$(($hi << 2 | 0x80 | $lo))
		address="${BT_OID}$(printf "%6.6X" $address)"
		address=$(echo $address | sed -r 's/(..)(..)(..)(..)(..)(..)/\1:\2:\3:\4:\5:\6/')
	fi
	echo $address
}


# Bluetooth
# note: the bluetooth module must be insmod'ed after wifi or else the
# chip firmware is not loaded properly and bluetooth will fail to work
if [ -f "${DEVICE_ID}" ]; then
	BG_MAC_ARG="bt_mac=$(get_bt_mac_address)"
else
	echo "warning device id file not found: ${DEVICE_ID}"
fi

echo "modprobe bt8xxx ${BG_MAC_ARG} cal_cfg=${BT_CAL} psmode=0"
modprobe bt8xxx ${BG_MAC_ARG} cal_cfg=${BT_CAL} psmode=0

/etc/init.d/bluetooth.sh &
