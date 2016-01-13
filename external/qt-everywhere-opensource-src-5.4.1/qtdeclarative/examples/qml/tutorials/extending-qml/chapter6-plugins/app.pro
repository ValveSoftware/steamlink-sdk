TARGET = chapter6-plugins
QT += qml quick

# Avoid going to debug/release subdirectory
# so that our application will see the
# import path for the Charts module.
win32: DESTDIR = ./

SOURCES += main.cpp
RESOURCES += app.qrc
