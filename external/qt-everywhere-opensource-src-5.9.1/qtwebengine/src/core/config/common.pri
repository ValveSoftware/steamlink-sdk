# Shared configuration for all our supported platforms

gn_args += \
    use_qt=true \
    is_component_build=false \
    is_shared=true \
    enable_media_router=false \
    enable_nacl=false \
    enable_remoting=false \
    enable_web_speech=false \
    use_experimental_allocator_shim=false \
    use_allocator=\"none\" \
    v8_use_external_startup_data=false \
    treat_warnings_as_errors=false

use?(printing) {
    gn_args += enable_basic_printing=true enable_print_preview=true
} else {
    gn_args += enable_basic_printing=false enable_print_preview=false
}

use?(pdf) {
    gn_args += enable_pdf=true
} else {
    gn_args += enable_pdf=false
}

use?(pepper_plugins) {
    gn_args += enable_plugins=true enable_widevine=true
} else {
    gn_args += enable_plugins=false enable_widevine=false
}

use?(spellchecker) {
    gn_args += enable_spellcheck=true
} else {
    gn_args += enable_spellcheck=false
}

use?(webrtc) {
    gn_args += enable_webrtc=true
} else {
    gn_args += enable_webrtc=false
}

use?(proprietary_codecs): gn_args += proprietary_codecs=true ffmpeg_branding=\"Chrome\"

CONFIG(release, debug|release) {
    force_debug_info: gn_args += symbol_level=1
    else: gn_args += symbol_level=0
}

!webcore_debug: gn_args += remove_webcore_debug_symbols=true
!v8base_debug: gn_args += remove_v8base_debug_symbols=true

# Compiling with -Os makes a huge difference in binary size
contains(WEBENGINE_CONFIG, reduce_binary_size): gn_args += optimize_for_size=true
