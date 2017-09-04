option(host_build)
QT       = core qmldevtools-private
SOURCES += main.cpp

QMAKE_TARGET_PRODUCT = qmlmin
QMAKE_TARGET_DESCRIPTION = QML/JS minifier

win32 {
   VERSION = $${QT_VERSION}.0
} else {
   VERSION = $${QT_VERSION}
}

load(qt_tool)
