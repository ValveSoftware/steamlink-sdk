GYP_ARGS += "-D qt_os=\"embedded_qnx\" -I config/embedded_qnx.gypi"

GYP_CONFIG += \
    disable_nacl=1 \
    enable_plugins=0 \
    enable_webrtc=0 \
    use_ash=0 \
    use_aura=1 \
    use_ozone=1 \
