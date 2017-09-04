TEMPLATE = aux

!debug_and_release: CONFIG += release

isQtMinimum(5, 8) {
    include($$QTWEBENGINE_OUT_ROOT/qtwebengine-config.pri)
    QT_FOR_CONFIG += webengine-private
}

build_pass|!debug_and_release {
    !qtConfig(system-ninja): CONFIG(release, debug|release) {
        out = $$ninjaPath()
        # check if it is not already build
        !exists($$out) {
            mkpath($$dirname(out))
            src_3rd_party_dir = $$absolute_path("$${getChromiumSrcDir()}/../", "$$QTWEBENGINE_ROOT")
            ninja_configure =  $$system_quote($$system_path($$absolute_path(ninja/configure.py, $$src_3rd_party_dir)))
            !system("cd $$system_quote($$system_path($$dirname(out))) && $$pythonPathForSystem() $$ninja_configure --bootstrap") {
                error("NINJA build error!")
            }
        }
    QMAKE_DISTCLEAN += $$out
    }
}

