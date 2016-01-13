TARGET = $$QTWEBENGINEPROCESS_NAME
TEMPLATE = app

# Needed to set LSUIElement=1
QMAKE_INFO_PLIST = Info_mac.plist

load(qt_build_paths)
contains(QT_CONFIG, qt_framework) {
    # Deploy the QtWebEngineProcess app bundle into the QtWebEngineCore framework.
    DESTDIR = $$MODULE_BASE_OUTDIR/lib/QtWebEngineCore.framework/Versions/5/Helpers

    # FIXME: We can remove those steps in Qt 5.5 once @rpath works
    # "QT += webenginecore" would pull all dependencies that we'd also need to update
    # with install_name_tool on OSX, but we only need access to the private
    # QtWebEngine::processMain. qtAddModule will take care of finding where
    # the library is without pulling additional librarie.
    QT = core
    qtAddModule(webenginecore, LIBS)
    CONFIG -= link_prl
    QMAKE_POST_LINK = \
        "xcrun install_name_tool -change " \
        "`xcrun otool -X -L $(TARGET) | grep QtWebEngineCore | cut -d ' ' -f 1` " \
        "@executable_path/../../../../QtWebEngineCore " \
        "$(TARGET); " \
        "xcrun install_name_tool -change " \
        "`xcrun otool -X -L $(TARGET) | grep QtCore | cut -d ' ' -f 1` " \
        "@executable_path/../../../../../../../QtCore.framework/QtCore " \
        "$(TARGET) "
} else {
    CONFIG -= app_bundle
    DESTDIR = $$MODULE_BASE_OUTDIR/libexec

    QT_PRIVATE += webenginecore
}

INCLUDEPATH += ../core

SOURCES = main.cpp

contains(QT_CONFIG, qt_framework) {
    target.path = $$[QT_INSTALL_LIBS]/QtWebEngineCore.framework/Versions/5/Helpers
} else {
    target.path = $$[QT_INSTALL_LIBEXECS]
}
INSTALLS += target
