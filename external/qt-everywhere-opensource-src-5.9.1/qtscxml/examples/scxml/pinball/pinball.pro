QT += widgets scxml

CONFIG += c++11

STATECHARTS = pinball.scxml

SOURCES += \
    main.cpp \
    mainwindow.cpp

FORMS += \
    mainwindow.ui

HEADERS += \
    mainwindow.h

# install
target.path = $$[QT_INSTALL_EXAMPLES]/scxml/pinball
INSTALLS += target
