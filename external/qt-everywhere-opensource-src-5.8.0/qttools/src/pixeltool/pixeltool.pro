QT += core-private gui-private widgets

mac {
    QMAKE_INFO_PLIST=Info_mac.plist
}

SOURCES += main.cpp qpixeltool.cpp
HEADERS += qpixeltool.h

load(qt_app)
