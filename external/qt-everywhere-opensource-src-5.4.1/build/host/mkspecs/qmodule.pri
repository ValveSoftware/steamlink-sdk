CONFIG +=  cross_compile force_debug_info compile_examples qpa largefile precompile_header use_gold_linker pcre
QT_BUILD_PARTS += libs
QT_SKIP_MODULES +=  qtlocation qtquick1 qtquickcontrols qtscript qtsensors qtserialport qtsvg qtwebchannel qtwebengine qtwebkit qtwebsockets qtxmlpatterns
QT_NO_DEFINES =  ALSA CUPS EGL_X11 IMAGEFORMAT_JPEG OPENVG STYLE_GTK XRENDER ZLIB
QT_QCONFIG_PATH = 
host_build {
    QT_CPU_FEATURES.x86_64 =  mmx sse sse2
} else {
    QT_CPU_FEATURES.arm = 
}
QT_COORD_TYPE = double
QT_LFLAGS_ODBC   = -lodbc
QMAKE_STRIP = armv7a-cros-linux-gnueabi-strip
styles += mac fusion windows
DEFINES += QT_NO_MTDEV
QT_CFLAGS_GLIB = -pthread -I/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/include/glib-2.0 -I/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/lib/glib-2.0/include
QT_LIBS_GLIB = -lgthread-2.0 -pthread -lglib-2.0
QT_CFLAGS_PULSEAUDIO = -D_REENTRANT -I/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/include/glib-2.0 -I/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/lib/glib-2.0/include
QT_LIBS_PULSEAUDIO = -lpulse-mainloop-glib -lpulse -lglib-2.0
QMAKE_CFLAGS_FONTCONFIG = -I/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/valve/marvell/steamlink-fw/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/home/qwhuang/bg2cd/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_gtb/usr/include -I/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/valve/marvell/steamlink-fw/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/include/freetype2 -I/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/home/qwhuang/bg2cd/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_gtb/usr/include -I/home/saml/dev/steamlink/firmware/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs_valve/usr/include/freetype2
QMAKE_LIBS_FONTCONFIG = -lfontconfig -lfreetype
sql-drivers = 
sql-plugins =  sqlite
