QT += qml testlib
osx:CONFIG -= app_bundle
CONFIG -= debug_and_release_target
INCLUDEPATH += ../../shared
SOURCES += qqmldebugjsserver.cpp
DEFINES += QT_QML_DEBUG_NO_WARNING

DESTDIR = ../qqmldebugjs

target.path = $$[QT_INSTALL_TESTS]/tst_qqmldebugjs
INSTALLS += target

