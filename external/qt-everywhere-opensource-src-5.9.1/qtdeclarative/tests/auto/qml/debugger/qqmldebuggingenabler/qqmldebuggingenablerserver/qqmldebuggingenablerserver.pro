QT += qml testlib
osx:CONFIG -= app_bundle
CONFIG -= debug_and_release_target
INCLUDEPATH += ../../shared
SOURCES += qqmldebuggingenablerserver.cpp
DEFINES += QT_QML_DEBUG_NO_WARNING

DESTDIR = ../qqmldebuggingenabler

target.path = $$[QT_INSTALL_TESTS]/tst_qqmldebuggingenabler
INSTALLS += target
