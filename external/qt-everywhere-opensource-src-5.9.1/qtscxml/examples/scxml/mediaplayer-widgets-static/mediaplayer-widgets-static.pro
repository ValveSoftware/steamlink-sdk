QT += widgets scxml

CONFIG += c++11

STATECHARTS = ../mediaplayer-common/mediaplayer.scxml

SOURCES += \
    mediaplayer-widgets-static.cpp \
    ../mediaplayer-common/mainwindow.cpp

FORMS += \
    ../mediaplayer-common/mainwindow.ui

HEADERS += \
    ../mediaplayer-common/mainwindow.h

# install
target.path = $$[QT_INSTALL_EXAMPLES]/scxml/mediaplayer-widgets-static
INSTALLS += target
