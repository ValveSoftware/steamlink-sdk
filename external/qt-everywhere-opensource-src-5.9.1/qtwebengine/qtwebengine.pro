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

QMAKE_DISTCLEAN += .qmake.cache

OTHER_FILES = \
    tools/buildscripts/* \
    tools/scripts/* \
    config.tests/khr/* \
    config.tests/libcap/* \
    config.tests/libvpx/* \
    config.tests/snappy/* \
    config.tests/srtp/* \
    mkspecs/features/*
