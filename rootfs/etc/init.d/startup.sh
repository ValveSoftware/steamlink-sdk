#!/bin/sh

RotateLogs()
{
	i=4
	while [ $i -gt 0 ]; do
		if [ -f "$1.$i" ]; then
			mv "$1.$i" "$1.$(($i + 1))"
		fi
		i=$(($i - 1))
	done
	if [ -f "$1" ]; then
		mv "$1" "$1.1"
	fi
}

STARTUPDIR=/etc/init.d/startup
if [ -f /mnt/config/system/startup_log_directory.txt ]; then
	STARTUPLOGDIR=`cat /mnt/config/system/startup_log_directory.txt`
else
	STARTUPLOGDIR=/mnt/scratch/log
fi

# check if we're a privileged user
if [ ! `id -u` = 0 ]; then 
	echo "startup.sh must be run by root, exit!"
	exit 0
fi

if [ ! -d "$STARTUPDIR" ]; then
	echo "$STARTUPDIR is not exist, exit!"
	exit
fi

if [ "$STARTUPLOGDIR" != "" ]; then
	mkdir -p $STARTUPLOGDIR
fi

for script in $STARTUPDIR/S*; do
	script_run=$script
	script_name=${script##*/}
	
	[ ! -f "$script_run" ] && continue
	
	echo -n "Run Script: $script_name"
	if [ ! -x "$script_run" ]; then
		echo -e "\t\t[ NOT EXECUTABLE, PASS IT ]"
		continue
	fi
	
	if [ "$STARTUPLOGDIR" != "" ]; then
		OUTPUT=$STARTUPLOGDIR/$script_name
		RotateLogs $OUTPUT
	else
		OUTPUT=/dev/null
	fi

	# run the script
	$script_run 1>$OUTPUT 2>&1
	if [ $? -eq 0 ]; then
		echo -e "\t\t[ OK ]"
	else
		echo -e "\t\t[ FAILED ]"
	fi
done
