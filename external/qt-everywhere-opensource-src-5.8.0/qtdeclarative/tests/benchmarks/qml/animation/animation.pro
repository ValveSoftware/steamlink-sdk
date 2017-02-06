CONFIG += benchmark
TEMPLATE = app
TARGET = tst_animation
QT += qml testlib core-private gui-private qml-private quick-private
macx:CONFIG -= app_bundle

SOURCES += tst_animation.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"
