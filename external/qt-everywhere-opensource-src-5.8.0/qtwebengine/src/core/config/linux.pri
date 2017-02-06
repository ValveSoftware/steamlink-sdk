include(common.pri)
include($$QTWEBENGINE_OUT_ROOT/qtwebengine-config.pri)
QT_FOR_CONFIG += gui-private webengine-private

# linux_use_bundled_gold currently relies on a hardcoded relative path from chromium/src/out/(Release|Debug)
# Disable it along with the -Wl,--threads flag just in case gold isn't installed on the system.
GYP_CONFIG += \
    linux_use_bundled_gold=0 \
    linux_use_bundled_binutils=0 \
    linux_use_gold_flags=0 \

GYP_CONFIG += \
    toolkit_uses_gtk=0 \
    use_ash=0 \
    use_aura=1 \
    use_cairo=0 \
    use_clipboard_aurax11=0 \
    use_cups=0 \
    use_gconf=0 \
    use_gio=0 \
    use_gnome_keyring=0 \
    use_kerberos=0 \
    use_pango=0 \
    use_openssl=1

use?(nss) {
    GYP_CONFIG += \
        use_nss_certs=1 \
        use_nss_verifier=1 \
        use_openssl_certs=0
} else {
    GYP_CONFIG += \
        use_nss_certs=0 \
        use_nss_verifier=0 \
        use_openssl_certs=1
}

gcc:!clang: greaterThan(QT_GCC_MAJOR_VERSION, 5): GYP_CONFIG += no_delete_null_pointer_checks=1

qtConfig(system-zlib): use?(system_minizip): GYP_CONFIG += use_system_zlib=1
qtConfig(system-png): GYP_CONFIG += use_system_libpng=1
qtConfig(system-jpeg): GYP_CONFIG += use_system_libjpeg=1
qtConfig(system-harfbuzz): use?(system_harfbuzz): GYP_CONFIG += use_system_harfbuzz=1
!qtConfig(glib): GYP_CONFIG += use_glib=0
qtConfig(pulseaudio) {
    GYP_CONFIG += use_pulseaudio=1
} else {
    GYP_CONFIG += use_pulseaudio=0
}
qtConfig(alsa) {
    GYP_CONFIG += use_alsa=1
} else {
    GYP_CONFIG += use_alsa=0
}
use?(system_libevent): GYP_CONFIG += use_system_libevent=1
use?(system_libwebp):  GYP_CONFIG += use_system_libwebp=1
use?(system_libsrtp):  GYP_CONFIG += use_system_libsrtp=1
use?(system_libxslt):  GYP_CONFIG += use_system_libxml=1
use?(system_jsoncpp):  GYP_CONFIG += use_system_jsoncpp=1
use?(system_opus):     GYP_CONFIG += use_system_opus=1
use?(system_snappy):   GYP_CONFIG += use_system_snappy=1
use?(system_vpx):      GYP_CONFIG += use_system_libvpx=1
use?(system_icu):      GYP_CONFIG += use_system_icu=1 icu_use_data_file_flag=0
use?(system_ffmpeg):   GYP_CONFIG += use_system_ffmpeg=1
use?(system_protobuf): GYP_CONFIG += use_system_protobuf=1
