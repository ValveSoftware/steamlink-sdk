!host_build {
    QMAKE_CFLAGS    += --sysroot=$$[QT_SYSROOT]
    QMAKE_CXXFLAGS  += --sysroot=$$[QT_SYSROOT]
    QMAKE_LFLAGS    += --sysroot=$$[QT_SYSROOT]
}
host_build {
    QT_ARCH = i386
    QT_BUILDABI = i386-little_endian-ilp32
    QT_TARGET_ARCH = arm
    QT_TARGET_BUILDABI = arm-little_endian-ilp32-eabi-hardfloat
} else {
    QT_ARCH = arm
    QT_BUILDABI = arm-little_endian-ilp32-eabi-hardfloat
}
QT.global.enabled_features = cross_compile shared rpath c++11 c++14 concurrent pkg-config
QT.global.disabled_features = framework appstore-compliant debug_and_release simulator_and_device build_all c++1z force_asserts separate_debug_info static
PKG_CONFIG_SYSROOT_DIR = /home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve
PKG_CONFIG_LIBDIR = /home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/lib/pkgconfig:/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/local/Qt-5.8.0/lib/pkgconfig
CONFIG += cross_compile shared release
QT_CONFIG += shared rpath release c++11 c++14 concurrent dbus mremap reduce_exports stl
QT_VERSION = 5.9.1
QT_MAJOR_VERSION = 5
QT_MINOR_VERSION = 9
QT_PATCH_VERSION = 1
QT_GCC_MAJOR_VERSION = 4
QT_GCC_MINOR_VERSION = 9
QT_GCC_PATCH_VERSION = 0
QT_EDITION = OpenSource
