GYP_ARGS += "-D qt_os=\"embedded_linux\" -I config/embedded_linux.gypi"

include(linux.pri)

GYP_CONFIG += \
    clang=0 \
    desktop_linux=0 \
    disable_nacl=1 \
    embedded=1 \
    enable_autofill_dialog=0 \
    enable_automation=0 \
    enable_basic_printing=0 \
    enable_captive_portal_detection=0 \
    enable_extensions=0 \
    enable_google_now=0 \
    enable_language_detection=0 \
    enable_managed_users=0 \
    enable_pdf=0 \
    enable_plugin_installation=0 \
    enable_plugins=0 \
    enable_print_preview=0 \
    enable_session_service=0 \
    enable_task_manager=0 \
    enable_themes=0 \
    enable_webrtc=0 \
    gtest_target_type=none \
    host_clang=0 \
    notifications=0 \
    ozone_auto_platforms=0 \
    ozone_platform_dri=0 \
    ozone_platform_test=0 \
    p2p_apis=0 \
    safe_browsing=0 \
    toolkit_views=1 \
    use_custom_freetype=0 \
    use_libpci=0 \
    use_ozone=1 \
    use_system_fontconfig=1 \
    use_x11=0 \
    v8_use_snapshot=false \
    want_separate_host_toolset=1 \
    angle_enable_gl=0 \
    use_system_expat=0 \

WEBENGINE_CONFIG *= reduce_binary_size
