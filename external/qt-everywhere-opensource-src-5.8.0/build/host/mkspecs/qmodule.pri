host_build {
    QT_CPU_FEATURES.x86_64 = mmx sse sse2
} else {
    QT_CPU_FEATURES.arm = neon
}
QT.global_private.enabled_features = dbus dbus-linked gui libudev posix_fallocate reduce_exports system-zlib widgets
QT.global_private.disabled_features = sse2 private_tests reduce_relocations release_tools
PKG_CONFIG_EXECUTABLE = /usr/bin/pkg-config
QMAKE_LIBS_DBUS = -ldbus-1
QMAKE_INCDIR_DBUS = /home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/include/dbus-1.0 /home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/lib/dbus-1.0/include
QMAKE_LIBS_LIBUDEV = -ludev
QT_COORD_TYPE = double
QMAKE_LIBS_ZLIB = -lz
CONFIG += use_gold_linker cross_compile compile_examples enable_new_dtags force_debug_info largefile neon precompile_header
QT_BUILD_PARTS += libs
QT_HOST_CFLAGS_DBUS += -I/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/include/dbus-1.0 -I/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/lib/dbus-1.0/include
