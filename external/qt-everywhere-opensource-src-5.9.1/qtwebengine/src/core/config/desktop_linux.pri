include(linux.pri)

gn_args += \
    use_sysroot=false \
    enable_session_service=false \
    enable_notifications=false \
    toolkit_views=false

use?(icecc) {
    gn_args += use_debug_fission=false
}

!use_gold_linker: gn_args += use_gold=false
