host_build {
    QT_CPU_FEATURES.x86_64 = mmx sse sse2
} else {
    QT_CPU_FEATURES.arm = neon
}
QT.global_private.enabled_features = alloca_h alloca dbus dbus-linked dlopen gui libudev network posix_fallocate reduce_exports relocatable sql system-zlib testlib widgets xml
QT.global_private.disabled_features = private_tests sse2 alloca_malloc_h android-style-assets avx2 gc_binaries reduce_relocations release_tools stack-protector-strong zstd
PKG_CONFIG_EXECUTABLE = /usr/bin/pkg-config
QMAKE_LIBS_DBUS = -ldbus-1
QMAKE_INCDIR_DBUS = /home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/include/dbus-1.0 /home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/lib/dbus-1.0/include
QMAKE_LIBS_LIBDL = -ldl
QMAKE_LIBS_LIBUDEV = -ludev
QT_COORD_TYPE = double
QMAKE_LIBS_ZLIB = -lz
CONFIG += cross_compile compile_examples enable_new_dtags force_debug_info largefile neon precompile_header
QT_BUILD_PARTS += libs
QT_HOST_CFLAGS_DBUS += -I/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/include/dbus-1.0 -I/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/lib/dbus-1.0/include
