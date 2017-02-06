include(../shared.pri)

TEMPLATE = lib
TARGET   = simpleqmlax

QT += widgets axserver quick quickwidgets

SOURCES = main.cpp
RC_FILE  = simpleqml.rc
DEF_FILE = simpleqml.def
RESOURCES = simpleqml.qrc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/activeqt/simpleqml
INSTALLS += target
