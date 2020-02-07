!host_build {
    QMAKE_CFLAGS    += --sysroot=$$[QT_SYSROOT]
    QMAKE_CXXFLAGS  += --sysroot=$$[QT_SYSROOT]
    QMAKE_LFLAGS    += --sysroot=$$[QT_SYSROOT]
}
host_build {
    QT_ARCH = x86_64
    QT_BUILDABI = x86_64-little_endian-lp64
    QT_TARGET_ARCH = arm
    QT_TARGET_BUILDABI = arm-little_endian-ilp32-eabi-hardfloat
} else {
    QT_ARCH = arm
    QT_BUILDABI = arm-little_endian-ilp32-eabi-hardfloat
}
QT.global.enabled_features = shared cross_compile shared rpath c++11 c++14 c99 c11 thread future concurrent pkg-config signaling_nan
QT.global.disabled_features = framework appstore-compliant debug_and_release simulator_and_device build_all c++1z c++2a force_asserts separate_debug_info static
PKG_CONFIG_SYSROOT_DIR = /home/saml/steamlink-sdk/rootfs
PKG_CONFIG_LIBDIR = /home/saml/steamlink-sdk/rootfs/usr/lib/pkgconfig:/home/saml/steamlink-sdk/rootfs/usr/lib/arm-linux-gnueabihf/pkgconfig:/home/saml/steamlink-sdk/rootfs/usr/local/Qt-5.14.1/lib/pkgconfig
QT_CONFIG += shared shared rpath release c++11 c++14 concurrent dbus reduce_exports stl
CONFIG += shared cross_compile shared release
QT_VERSION = 5.14.1
QT_MAJOR_VERSION = 5
QT_MINOR_VERSION = 14
QT_PATCH_VERSION = 1
QT_GCC_MAJOR_VERSION = 9
QT_GCC_MINOR_VERSION = 2
QT_GCC_PATCH_VERSION = 0
QT_EDITION = OpenSource
