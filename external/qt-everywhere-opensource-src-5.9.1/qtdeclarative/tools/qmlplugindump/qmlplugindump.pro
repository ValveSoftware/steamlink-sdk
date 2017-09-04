QT += qml qml-private quick-private core-private
qtHaveModule(widgets): QT += widgets

CONFIG += no_import_scan

QTPLUGIN.platforms = qminimal

SOURCES += \
    main.cpp \
    qmlstreamwriter.cpp \
    qmltypereader.cpp

HEADERS += \
    qmlstreamwriter.h \
    qmltypereader.h

macx {
    # Prevent qmlplugindump from popping up in the dock when launched.
    # We embed the Info.plist file, so the application doesn't need to
    # be a bundle.
    QMAKE_LFLAGS += -Wl,-sectcreate,__TEXT,__info_plist,$$shell_quote($$PWD/Info.plist)
    CONFIG -= app_bundle
}

QMAKE_TARGET_PRODUCT = qmlplugindump
QMAKE_TARGET_DESCRIPTION = QML plugin dump tool

win32 {
   VERSION = $${QT_VERSION}.0
} else {
   VERSION = $${QT_VERSION}
}

load(qt_tool)
