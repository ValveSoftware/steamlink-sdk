TARGET = $$QTWEBENGINEPROCESS_NAME

# Needed to set LSUIElement=1
QMAKE_INFO_PLIST = Info_mac.plist

QT += webenginecore

INCLUDEPATH += ../core

SOURCES = main.cpp

win32 {
    SOURCES += \
        support_win.cpp
}

load(qt_app)

qtConfig(build_all): CONFIG += build_all

qtConfig(framework) {
    # Deploy the QtWebEngineProcess app bundle into the QtWebEngineCore framework.
    DESTDIR = $$MODULE_BASE_OUTDIR/lib/QtWebEngineCore.framework/Versions/5/Helpers
} else {
    CONFIG -= app_bundle
    win32: DESTDIR = $$MODULE_BASE_OUTDIR/bin
    else:  DESTDIR = $$MODULE_BASE_OUTDIR/libexec
}
msvc: QMAKE_LFLAGS *= /LARGEADDRESSAWARE

qtConfig(framework) {
    target.path = $$[QT_INSTALL_LIBS]/QtWebEngineCore.framework/Versions/5/Helpers
} else {
    target.path = $$[QT_INSTALL_LIBEXECS]
}
