load(qt_build_config)
load(qt_parts)

isPlatformSupported() {
   !exists(src/3rdparty/chromium): \
       error("Submodule qtwebengine-chromium does not exist. Run 'git submodule update --init'.")
   load(configure)
   runConfigure()
}

!isEmpty(skipBuildReason) {
    SUBDIRS =
    log($${skipBuildReason}$${EOL})
    log(QtWebEngine will not be built.$${EOL})
}

OTHER_FILES = \
    tools/buildscripts/* \
    tools/scripts/* \
    tools/qmake/config.tests/khr/* \
    tools/qmake/config.tests/libcap/* \
    tools/qmake/config.tests/libvpx/* \
    tools/qmake/config.tests/snappy/* \
    tools/qmake/config.tests/srtp/* \
    tools/qmake/mkspecs/features/*
