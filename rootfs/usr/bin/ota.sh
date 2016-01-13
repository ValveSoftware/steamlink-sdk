#!/bin/sh

cmd=$1

cmd_update()
{
	OTA_FILENAME=$1
	OTA_PATH=/mnt/scratch/$OTA_FILENAME

	# OTA file must be in "cache" partition
	if [ ! -f "$OTA_PATH" ]; then
		echo "No $OTA_PATH ?"
		echo "Update file has to be in /mnt/scratch"
		exit 1
	fi

	fts-set bootloader.command boot-recovery
	fts-set recovery_args "recovery" "--update_package=/cache/$OTA_FILENAME"

	echo "Ready to reboot"
	echo "Try \"reboot -f\""
}

cmd_factory_reset()
{
	fts-set bootloader.command boot-recovery
	fts-set recovery_args "recovery" "--factory"

	echo "Ready to reboot"
	echo "Try \"reboot -f\""
}

usage()
{
	echo "ota.sh update <filename>"
	echo "ota.sh factory-reset"
}

case $cmd in
	update)
		cmd_update $2
		;;
	factory-reset)
		cmd_factory_reset
		;;
	*)
		usage
		;;
esac

