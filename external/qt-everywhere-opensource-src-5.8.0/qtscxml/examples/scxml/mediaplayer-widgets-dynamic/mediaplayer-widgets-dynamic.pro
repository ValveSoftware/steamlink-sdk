QT += widgets scxml

CONFIG += c++11

SOURCES += \
    mediaplayer-widgets-dynamic.cpp \
    ../mediaplayer-common/mainwindow.cpp

FORMS += \
    ../mediaplayer-common/mainwindow.ui

HEADERS += \
    ../mediaplayer-common/mainwindow.h

RESOURCES += \
    mediaplayer.qrc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/scxml/mediaplayer-widgets-dynamic
INSTALLS += target
