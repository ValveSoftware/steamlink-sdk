
# SDK for the Valve Steam Link

This is the SDK for the Valve Steam Link

## Hardware

The Steam Link hardware is a single core ARMv7 processor using the hard-float ABI,
running at `1 GHz`, with neon instruction support. It has approximately `256 MB` of available
RAM and `1 GB` of usable flash storage.


## Software

The Steam Link software is custom Linux firmware based on `kernel 3.8` and `glibc 2.19`.

### API support
The Steam Link SDK has support for the following major APIs:

- `OpenGL ES 2.0`
- `Qt 5.14.1`
- `SDL 2.0`

The SDL game controller API is recommended for Steam Controller support on the Steam Link.


## Repository Contents
### examples
Examples to demonstrate how to build applications using the SDK. Each application directory has a `build_steamlink.sh` script to build and package the example.

Native app icons should be 138x138, and will be scaled to that size if needed.

### external
Source code to 3rd party components of the Steam Link, each directory has a `build_steamlink.sh` script to rebuild the component.

### kernel
Steam Link linux kernel source code

### rootfs
Steam Link root filesystem

Debug symbols for binaries in the latest build are available from:
http://media.steampowered.com/steamlink/steamlink-sdk-symbols.tar.xz


### toolchain
GCC toolchain for Steam Link

### setenv.sh
Script to configure build environment


## Building

Steam Link applications are built using this SDK on Linux.

To set up the build environment, source the top level `setenv.sh`.
```Bash
# source setenv.sh
```
This is done inside the build script for each example, as needed.


## Deploying

Put the files in a folder with the app name on a FAT32 USB drive under `\steamlink\apps`, insert it into the Steam Link and power cycle the device.

The app will be copied onto the system at boot and can be launched from the menu.


## SSH Access

You may need to enable ssh access to the Steam Link for advanced debugging.
You can do this by putting a file called `enable_ssh.txt` on a FAT32 USB drive under `\steamlink\config\system`, inserting it into the Steam Link and power cycle the device.

The root password is `steamlink123` and should be changed using the `passwd` command the first time you log in.

SSH access will remain enabled until a factory reset.


## Debugging

Recommended way is to run `gdbserver` on the Steam Link and use a local `gdb` to connect to it and debug an application.

### Preparation

First, configure environment for SDK:
```Bash
# source setenv.sh
```

Second, make sure that the install path of the SDK is on gdb's `auto-load-safe-path` by:
```Bash
# echo "add-auto-load-safe-path $MARVELL_SDK_PATH" >> ~/.gdbinit
```

Build an example application and transfer it into the steam link:
```Bash
# cd examples/linux
# make
# scp example root@10.1.211.126:/mnt/scratch
```

On the steam link, run the application:
```Bash
# gdbserver :8080 example
```

Or you can attach to a running process with:
```Bash
# gdbserver --attach :8080 $(pidof example)
```

On the host, create a `.gdbinit` file for your application and target:
```Bash
$ ../../scripts/create_gdbinit.sh example 10.1.211.126:8080
```
On the host, run `gdb` and attach to the gdbserver:
```Bash
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
Breakpoint 1, __printf (format=0xb6f8c764 "Hello world!\n") at printf.c:28
28	printf.c: No such file or directory.
(gdb) where
#0  __printf (format=0xb6f8c764 "Hello world!\n") at printf.c:28
#1  0xb6f8c704 in main (argc=1, argv=0xbed64e64) at example.c:7
```

## Building the kernel
Follow these steps in addition to setting up the build environment as described above.

WARNING: Steam Link devices will only boot with a kernel signed by Valve. If you attempt to replace the kernel with an unsigned binary you will void your warranty and render your Steam Link unbootable.

### Set environment variables for kernel build
```Bash
# export ARCH=arm; export LOCALVERSION="-mrvl"
```
### Configure kernel for Steam Link target
```Bash
#  make bg2cd_penguin_mlc_defconfig
```

### Customize kernel configuration (optional)
```Bash
# make menuconfig
```

### Build kernel
```Bash
# make
```

### Build modules (optional)
```Bash
# make modules
```
