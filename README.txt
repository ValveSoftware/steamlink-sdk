
This is the SDK for the Valve Steam Link


Hardware:
--------

The Steam Link hardware is a single core ARMv7 processor using the hard-float ABI,
running at 1 GHz, with neon instruction support. It has approximately 256 MB of available
RAM and 500 MB of usable flash storage.


Software:
--------

The Steam Link software is custom Linux firmware based on kernel 3.8 and glibc 2.19.

The Steam Link SDK has support for the following major APIs:

	OpenGL ES 2.0
	Qt 5.4
	SDL 2.0

The SDL game controller API is recommended for Steam Controller support on the Steam Link.


Contents:
--------

	examples 
		very simple example applications to demostrate how to build applications using the SDK
	external
		source code to 3rd party components of the Steam Link, each directory has a
		"build_soc.sh" script to rebuild the component.
	kernel
		Steam Link linux kernel source code
	rootfs
		Steam Link root filesystem
	toolchain
		GCC toolchain for Steam Link
	setenv.sh
		Script to configure build environment


Building:
--------

Steam Link applications are built using this SDK on Linux.

To set up the build environment, source the top level setenv.sh

# source setenv.sh

Go to the examples and look at their README.txt files to see how to build them


Deploying:
---------

Put the files in a folder with the app name on a USB drive under \steamlink\apps, insert it into the Steam Link and power cycle the device.

The app will be copied onto the system at boot and can be launched from the menu.


SSH Access:
----------

You may need to enable ssh access to the Steam Link for advanced debugging.
You can do this by putting a file called enable_ssh.txt on a USB drive under \steamlink\config\system, inserting it into the Steam Link and power cycle the device.

The root password is steamlink123 and should be changed using the passwd command the first time you log in.

SSH access will remain enabled until a factory reset.


Debugging:
---------

Recommended way is to run `gdbserver` on the Steam Link and use a local `gdb` to connect to it and debug an application.

Preparation

First, configure environment for SDK:

	# source setenv.sh

Second, make sure that the install path of the SDK is on gdb's `auto-load-safe-path` by:

	# echo "add-auto-load-safe-path $MARVELL_SDK_PATH" >> ~/.gdbinit

Build an example application and transfer it into the steam link:

	# cd examples/linux
	# make
	# scp example root@10.1.211.126:/mnt/scratch

On the steam link, run the application:

	# gdbserver :8080 example

	Or you can attach to a running process with:

	# gdbserver --attach :8080 $(pidof example)

On the host, create a .gdbinit file for your application and target:

	$ ../../scripts/create_gdbinit.sh example 10.1.211.126:8080

On the host, run gdb and attach to the gdbserver:

	ubuntu:/valve/marvell/sdk/examples/linux $ armv7a-cros-linux-gnueabi-gdb
	GNU gdb (Gentoo 7.7.1 p1 e02ddb0f2eea465662e99fee14e9c41f23769624) 7.7.0.20140425-cvs
	Copyright (C) 2014 Free Software Foundation, Inc.
	License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
	This is free software: you are free to change and redistribute it.
	There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
	and "show warranty" for details.
	This GDB was configured as "--host=x86_64-pc-linux-gnu --target=armv7a-cros-linux-gnueabi".
	Type "show configuration" for configuration details.
	For bug reporting instructions, please see:
	<http://bugs.gentoo.org/>.
	Find the GDB manual and other documentation resources online at:
	<http://www.gnu.org/software/gdb/documentation/>.
	For help, type "help".
	Type "apropos word" to search for commands related to "word".
	0xb6f6ba00 in _start () from /valve/marvell/sdk/rootfs/lib/ld-linux-armhf.so.3
	(gdb) break printf
	Function "printf" not defined.
	Make breakpoint pending on future shared library load? (y or [n]) y
	Breakpoint 1 (printf) pending.
	(gdb) c
	Continuing.

	Breakpoint 1, __printf (format=0xb6f8c764 "Hello world\n!") at printf.c:28
	28	printf.c: No such file or directory.
	(gdb) where
	#0  __printf (format=0xb6f8c764 "Hello world\n!") at printf.c:28
	#1  0xb6f8c704 in main (argc=1, argv=0xbed64e64) at example.c:7

