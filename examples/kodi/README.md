
#About

This script compiles Kodi version 16.1-Jarvis ([c327c53](https://github.com/xbmc/xbmc/commit/c327c53ac5346f71219e8353fe046e43e4d4a827)).
You can change this in `build_steamlink.sh` by providing different tag/branch or by removing `-b "16.1-Jarvis"` parameter,
however the build script can break.

It was tested on Arch Linux and Ubuntu 16.04.

##Building Kodi

Before compiling, make sure you have installed these packages:
```Bash
automake autopoint build-essential cmake curl default-jre doxygen gawk git gperf libcurl4-openssl-dev libtool swig unzip zip zlib1g-dev wget
```
For non-debianish distros:
- `build-essential` means `make`, `gcc`, etc.
- `default-jre` will be `jre8-openjdk` or something like that

This list might be incomplete. When your build fails on some dependency,
please install it with your package manager. You can find the list of all required
packages in [Kodi readme](https://github.com/xbmc/xbmc/blob/master/docs/README.linux#L46)
(some libraries are not required, since they will be built with the Steam Link SDK compiler from source).

##Executing the build process

1. Open new terminal window, **DO NOT** `source setenv.sh` - it will break the build environment!
2. `$ cd /path/to/steamlink-sdk/examples/kodi`
3. `$ ./build_steamlink.sh`
4. Go get some coffee

This should end with `Build complete!` message and you should have `steamlink` dir in your current directory.
Copy the `steamlink` directory to a USB flash drive, insert it into the Steam Link and
power cycle the device.

##What is working:
- Addons
- Pictures
- Video output
- Sound output (thanks to [@garbear](https://github.com/garbear/))

##What is not working:
- ...
