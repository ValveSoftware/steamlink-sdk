TEMPLATE = app
TARGET = iconextractor
CONFIG += console
QT = core gui winextras
LIBS += -lshell32 -luser32
SOURCES += main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/winextras/iconextractor
INSTALLS += target
