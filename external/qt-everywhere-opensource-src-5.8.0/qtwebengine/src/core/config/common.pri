# Shared configuration for all our supported platforms

# Trigger Qt-specific build conditions.
GYP_CONFIG += use_qt=1
# We do not want to ship more external binary blobs, so let v8 embed its startup data.
GYP_CONFIG += v8_use_external_startup_data=0
# WebSpeech requires Google API keys and adds dependencies on speex and flac.
GYP_CONFIG += enable_web_speech=0
# We do not use or even include the extensions
GYP_CONFIG += enable_extensions=0

sanitize_address: GYP_CONFIG += asan=1
sanitize_thread: GYP_CONFIG += tsan=1
sanitize_memory: GYP_CONFIG += msan=1
sanitize_undefined: GYP_CONFIG += ubsan=1
