TEMPLATE = app
TARGET = qmltime
QT += qml quick
QT += quick-private
macx:CONFIG -= app_bundle

QMAKE_TARGET_PRODUCT = qmltime
QMAKE_TARGET_DESCRIPTION = Tool for benchmarking the instantiation of a QML component

win32 {
   VERSION = $${QT_VERSION}.0
} else {
   VERSION = $${QT_VERSION}
}

SOURCES += qmltime.cpp
