include(../shared.pri)

TEMPLATE = lib
TARGET   = multipleax

CONFIG += warn_off dll
QT += widgets axserver

SOURCES  = main.cpp
HEADERS  = ax1.h ax2.h
RC_FILE  = multipleax.rc
DEF_FILE = multipleax.def

# install
target.path = $$[QT_INSTALL_EXAMPLES]/activeqt/multiple
INSTALLS += target
