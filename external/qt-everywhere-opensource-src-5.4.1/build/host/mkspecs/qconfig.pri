#configuration
CONFIG +=  cross_compile shared qpa no_mocdepend release qt_no_framework
host_build {
    QT_ARCH = x86_64
    QT_TARGET_ARCH = arm
} else {
    QT_ARCH = arm
    QMAKE_DEFAULT_LIBDIRS = /home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/lib /home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/lib /home/saml/dev/steamlink/firmware/MRVL/build_scripts/toplevel_scripts/armv7a/usr/lib/gcc/armv7a-cros-linux-gnueabi/4.9.x-google /home/saml/dev/steamlink/firmware/MRVL/build_scripts/toplevel_scripts/armv7a/usr/lib/gcc /home/saml/dev/steamlink/firmware/MRVL/build_scripts/toplevel_scripts/armv7a/usr/armv7a-cros-linux-gnueabi/lib
    QMAKE_DEFAULT_INCDIRS = /home/saml/dev/steamlink/firmware/MRVL/build_scripts/toplevel_scripts/armv7a/usr/lib/gcc/armv7a-cros-linux-gnueabi/4.9.x-google/include/g++-v4 /home/saml/dev/steamlink/firmware/MRVL/build_scripts/toplevel_scripts/armv7a/usr/lib/gcc/armv7a-cros-linux-gnueabi/4.9.x-google/include/g++-v4/armv7a-cros-linux-gnueabi /home/saml/dev/steamlink/firmware/MRVL/build_scripts/toplevel_scripts/armv7a/usr/lib/gcc/armv7a-cros-linux-gnueabi/4.9.x-google/include/g++-v4/backward /home/saml/dev/steamlink/firmware/MRVL/build_scripts/toplevel_scripts/armv7a/usr/lib/gcc/armv7a-cros-linux-gnueabi/4.9.x-google/include /home/saml/dev/steamlink/firmware/MRVL/build_scripts/toplevel_scripts/armv7a/usr/lib/gcc/armv7a-cros-linux-gnueabi/4.9.x-google/include-fixed /home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/include
}
QT_CONFIG +=  minimal-config small-config medium-config large-config full-config fontconfig libudev evdev c++11 accessibility egl eglfs opengl opengles2 shared qpa reduce_exports clock-gettime clock-monotonic posix_fallocate mremap getaddrinfo ipv6ifname getifaddrs inotify eventfd png system-freetype harfbuzz system-zlib nis iconv glib dbus openssl rpath alsa pulseaudio icu concurrent audio-backend release

#versioning
QT_VERSION = 5.4.1
QT_MAJOR_VERSION = 5
QT_MINOR_VERSION = 4
QT_PATCH_VERSION = 1

#namespaces
QT_LIBINFIX = 
QT_NAMESPACE = 

# pkgconfig
PKG_CONFIG_SYSROOT_DIR = /home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve
PKG_CONFIG_LIBDIR = /home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/lib/pkgconfig:/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/share/pkgconfig:/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/lib/armv7a-cros-linux-gnueabi/pkgconfig

# sysroot
!host_build {
    QMAKE_CFLAGS    += --sysroot=$$[QT_SYSROOT]
    QMAKE_CXXFLAGS  += --sysroot=$$[QT_SYSROOT]
    QMAKE_LFLAGS    += --sysroot=$$[QT_SYSROOT]
}

QT_GCC_MAJOR_VERSION = 4
QT_GCC_MINOR_VERSION = 9
QT_GCC_PATCH_VERSION = 
