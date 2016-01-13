TEMPLATE = app
TARGET = detect_grue
CONFIG += console
QT = core sensors

DESTDIR = $$OUT_PWD/..

SOURCES = main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/sensors/grue
INSTALLS += target
