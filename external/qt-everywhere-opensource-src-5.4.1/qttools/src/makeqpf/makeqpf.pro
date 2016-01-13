QT += widgets gui-private core-private
CONFIG += console
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

HEADERS += qpf2.h mainwindow.h
SOURCES += main.cpp qpf2.cpp mainwindow.cpp
DEFINES += QT_NO_FREETYPE
FORMS += mainwindow.ui
RESOURCES += makeqpf.qrc

load(qt_app)
