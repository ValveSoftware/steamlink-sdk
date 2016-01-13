TEMPLATE = app
TARGET   = simpleax

CONFIG += warn_off
QT += widgets axserver

SOURCES  = main.cpp
RC_FILE  = simple.rc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/activeqt/simple
INSTALLS += target
