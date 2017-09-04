GYP_ARGS += "-D qt_os=\"embedded_qnx\" -I config/embedded_qnx.gypi"

include(common.pri)

GYP_CONFIG += \
    disable_nacl=1 \
    enable_webrtc=0 \
    use_ash=0 \
    use_aura=1 \
    use_ozone=1 \
