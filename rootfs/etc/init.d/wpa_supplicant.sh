#!/bin/ash

################
# Debug options
#   Uncomment the following to enable debugging information and/or
#   wpa_cli control interface
################

#DEBUG_OPTIONS="-ddd"
#ENABLE_CTRL_INTERFACE=1


################
# Configuration file locations
################

FACTORY_DIR=/mnt/factory_setting
CONFIG_DIR=/mnt/config/system
SUPPLICANT_CONF_FILE=/tmp/wpa_supplicant.conf
SKU_DOMAIN_FILE=$FACTORY_DIR/sku_domain.txt
WIFI_BAND_CONF_FILE=${CONFIG_DIR}/wifi_band_selection.txt


################
# Gather parameters for configuration file
################

ctrl_interface_line=""
if [ "${ENABLE_CTRL_INTERFACE}" = "1" ]; then
	ctrl_interface_line="ctrl_interface=/var/run/wpa_supplicant"
fi

country_line="" 
if [ -f "$SKU_DOMAIN_FILE" ]; then
	country_line="country=$(cat $SKU_DOMAIN_FILE)"
fi

device_name=Steamlink

# The manufacturer of the device (up to 64 ASCII characters)
manufacturer=Valve

# Model of the device (up to 32 ASCII characters)
MODEL_NAME_FILE=${FACTORY_DIR}/product_model_number
model_name="EV21"
if [ -f "${MODEL_NAME_FILE}" ]; then
	model_name=$(head -1 ${MODEL_NAME_FILE} | sed -r 's/(.{32}).*/\1/')
fi

# Additional device description (up to 32 ASCII characters)                
MODEL_NUMBER_FILE=${FACTORY_DIR}/board_part_number
model_number="1"
if [ -f "${MODEL_NUMBER_FILE}" ]; then
	model_number=$(head -1 ${MODEL_NUMBER_FILE} | sed -r 's/(.{32}).*/\1/')
fi

# Serial number of the device (up to 32 characters)                    
SERIAL_NUMBER_FILE=${FACTORY_DIR}/box_serial
serial_number="12345678"
if [ -f "${SERIAL_NUMBER_FILE}" ]; then
	serial_number=$(head -1 ${SERIAL_NUMBER_FILE} | sed -r 's/(.{32}).*/\1/')
fi

device_type=1-0050F204-1
os_version=00000308
config_methods="display virtual_display virtual_push_button"
wps_cred_processing=0

band_selection_line=""
if [ -f "$WIFI_BAND_CONF_FILE" ]; then
	band_selection_line="band_selection=$(cat $WIFI_BAND_CONF_FILE)"
fi

################
# Generate configuration file
################

cat << __EOF__ > ${SUPPLICANT_CONF_FILE}
${ctrl_interface_line}
${country_line}
device_name=${device_name}
manufacturer=${manufacturer}
model_number=${model_number}
model_name=${model_name}
serial_number=${serial_number}
device_type=${device_type}
os_version=${os_version}
config_methods=${config_methods}
wps_cred_processing=${wps_cred_processing}
pmf=1
${band_selection_line}

__EOF__


################
# Start the daemon
# note: we are only enabling supplicant on mlan0 for now.  If we enable other
#       interfaces in the future, we would need to also add additional 
#       configuration files for those interfaces
################

restart_count=0
while [ 1 = 1 ]; do
	restart_count=$((${restart_count} + 1))
	# This script runs in the background. Start wpa_supplicant on the foreground to avoid having to run the pgrep in a background loop.
	# Instead just re-start supplicant when it quits.
	if [ ${restart_count} -ge 5 ]; then
		echo STARTING wpa_supplicant without config file
		# the config file might be corrupt, start without it
		wpa_supplicant -u ${DEBUG_OPTIONS} -Dnl80211 -imlan0
	else
		echo STARTING wpa_supplicant
		wpa_supplicant -u ${DEBUG_OPTIONS} -c${SUPPLICANT_CONF_FILE} -Dnl80211 -imlan0
	fi

	sleep 2
done

