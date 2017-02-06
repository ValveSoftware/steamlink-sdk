include(../shared.pri)

TEMPLATE = app
QT  += axserver

QT += widgets

# Input
SOURCES += main.cpp

RC_FILE  = comapp.rc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/activeqt/comapp
INSTALLS += target
