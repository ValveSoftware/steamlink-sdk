TEMPLATE   = app
QT        += widgets

HEADERS   += remotecontrol.h
SOURCES   += main.cpp \
             remotecontrol.cpp
FORMS     += remotecontrol.ui
RESOURCES += remotecontrol.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/assistant/remotecontrol
INSTALLS += target

