TEMPLATE = app
TARGET   = menusax

CONFIG += warn_off
QT += widgets axserver

SOURCES  = main.cpp menus.cpp
HEADERS  = menus.h
RC_FILE  = menus.rc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/activeqt/menus
INSTALLS += target
