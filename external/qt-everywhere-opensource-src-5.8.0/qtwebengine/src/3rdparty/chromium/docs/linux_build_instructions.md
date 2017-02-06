# Linux-specific build instructions

## Common checkout instructions

This page covers Linux-specific setup and configuration. The
[general checkout
instructions](http://dev.chromium.org/developers/how-tos/get-the-code) cover
installing depot tools and checking out the code via git.

## Overview

Due its complexity, Chromium uses a set of custom tools to check out and build
rather than configure/make like most projects. You _must_ use gclient and
ninja, and there is no "install" step provided.

### System requirements

*   **64-bits**: x86 builds are not supported on Linux.
*   **Memory**: >16GB is highly recommended.
*   **Disk space**: Expect a full checkout and build to take nearly 100GB.
*   **Distribution**: You should be able to build Chromium on any reasonably modern Linux
    distribution, but there are a lot of distributions and we sometimes break
    things on one or another. Internally, our development platform has been a
    variant of Ubuntu 14.04 (Trusty Tahr); we expect you will have the most
    luck on this platform.

## Software setup

Non-Ubuntu distributions are not officially supported for building and the
instructions below might be outdated.

### Ubuntu

Once you have checked out the code, run
[build/install-build-deps.sh](/build/install-build-deps.sh) The script only
supports current releases as listed on https://wiki.ubuntu.com/Releases.
This script is used to set up the canonical builders, and as such is the most
up to date reference for the required prerequisites.

### Debian

Follow the Ubuntu instructions above. If you want to install the build-deps
manually, note that the original packages are for Ubuntu. Here are the Debian
equivalents:

*   libexpat-dev -> libexpat1-dev
*   freetype-dev -> libfreetype6-dev
*   libbzip2-dev -> libbz2-dev
*   libcupsys2-dev -> libcups2-dev

Additionally, if you're building Chromium components for Android, you'll need to
install the package: lib32z1

### openSUSE

For openSUSE 11.0 and later, see
[Linux openSUSE Build Instructions](linux_open_suse_build_instructions.md).

### Fedora

Recent systems:

    su -c 'yum install subversion pkgconfig python perl gcc-c++ bison flex \
    gperf nss-devel nspr-devel gtk2-devel glib2-devel freetype-devel atk-devel \
    pango-devel cairo-devel fontconfig-devel GConf2-devel dbus-devel \
    alsa-lib-devel libX11-devel expat-devel bzip2-devel dbus-glib-devel \
    elfutils-libelf-devel libjpeg-devel mesa-libGLU-devel libXScrnSaver-devel \
    libgnome-keyring-devel cups-devel libXtst-devel libXt-devel pam-devel httpd \
    mod_ssl php php-cli wdiff'

The msttcorefonts packages can be obtained by following the instructions
present [here](http://www.fedorafaq.org/#installfonts). For the optional
packages:

*   php-cgi is provided by the php-cli package
*   wdiff doesn't exist in Fedora repositories, a possible alternative would be
    dwdiff
*   sun-java6-fonts doesn't exist in Fedora repositories, needs investigating

### Arch Linux

Most of these packages are probably already installed since they're often used,
and the parameter --needed ensures that packages up to date are not reinstalled.

    sudo pacman -S --needed python perl gcc gcc-libs bison flex gperf pkgconfig \
    nss alsa-lib gconf glib2 gtk2 nspr ttf-ms-fonts freetype2 cairo dbus \
    libgnome-keyring

For the optional packages on Arch Linux:

*   php-cgi is provided with pacman
*   wdiff is not in the main repository but dwdiff is. You can get wdiff in
    AUR/yaourt
*   sun-java6-fonts do not seem to be in main repository or AUR.

### Mandriva

    urpmi lib64fontconfig-devel lib64alsa2-devel lib64dbus-1-devel \
    lib64GConf2-devel lib64freetype6-devel lib64atk1.0-devel lib64gtk+2.0_0-devel \
    lib64pango1.0-devel lib64cairo-devel lib64nss-devel lib64nspr-devel g++ python \
    perl bison flex subversion gperf

*   msttcorefonts are not available, you will need to build your own (see
instructions, not hard to do, see
[mandriva_msttcorefonts.md](mandriva_msttcorefonts.md)) or use drakfont to
import the fonts from a windows installation
*  These packages are for 64 bit, to download the 32 bit packages,
substitute lib64 with lib
*  Some of these packages might not be explicitly necessary as they come as
   dependencies, there is no harm in including them however.

### Gentoo

    emerge www-client/chromium

## Troubleshooting

### Linker Crashes

If, during the final link stage:

    LINK out/Debug/chrome

You get an error like:

    collect2: ld terminated with signal 6 Aborted terminate called after throwing an
    instance of 'std::bad_alloc'

    collect2: ld terminated with signal 11 [Segmentation fault], core dumped

you are probably running out of memory when linking. You *must* use a 64-bit
system to build. Try the following build settings (see [GN build
configuration](https://www.chromium.org/developers/gn-build-configuration) for
setting):

*   Build in release mode (debugging symbols require more memory).
    `is_debug = false`
*   Turn off symbols. `symbol_level = 0`
*   Build in component mode (this is for developers only, it will be slower and
    may have broken functionality). `is_component_build = true`

## More links

*   [Faster builds on Linux](linux_faster_builds.md)
*   Information about [building with Clang](clang.md).
*   You may want to
    [use a chroot](using_a_linux_chroot.md) to
    isolate yourself from versioning or packaging conflicts (or to run the
    layout tests).
*   Cross-compiling for ARM? See [LinuxChromiumArm](linux_chromium_arm.md).
*   Want to use Eclipse as your IDE? See
    [LinuxEclipseDev](linux_eclipse_dev.md).
*   Built version as Default Browser? See
    [LinuxDevBuildAsDefaultBrowser](linux_dev_build_as_default_browser.md).

## Next Steps

If you want to contribute to the effort toward a Chromium-based browser for
Linux, please check out the [Linux Development page](linux_development.md) for
more information.
