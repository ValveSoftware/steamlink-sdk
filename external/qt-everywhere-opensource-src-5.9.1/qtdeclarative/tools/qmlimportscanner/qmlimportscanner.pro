option(host_build)

QT = core qmldevtools-private
DEFINES += QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII

SOURCES += main.cpp

QMAKE_TARGET_PRODUCT = qmlimportscanner
QMAKE_TARGET_DESCRIPTION = Tool to scan projects for QML imports

win32 {
   VERSION = $${QT_VERSION}.0
} else {
   VERSION = $${QT_VERSION}
}

load(qt_tool)
