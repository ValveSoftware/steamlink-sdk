TEMPLATE = app
TARGET   = openglax

CONFIG += warn_off
QT += widgets axserver opengl

HEADERS  = glbox.h \
           globjwin.h
SOURCES  = glbox.cpp \
           globjwin.cpp \
           main.cpp

RC_FILE = opengl.rc
DEF_FILE = opengl.def

# install
target.path = $$[QT_INSTALL_EXAMPLES]/activeqt/opengl
INSTALLS += target
