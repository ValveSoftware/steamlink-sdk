include(../shared.pri)

TEMPLATE = app
TARGET   = simpleax

CONFIG += warn_off
QT += widgets axserver

SOURCES  = main.cpp
RC_FILE  = simple.rc
DEF_FILE = simple.def

# install
target.path = $$[QT_INSTALL_EXAMPLES]/activeqt/simple
INSTALLS += target
