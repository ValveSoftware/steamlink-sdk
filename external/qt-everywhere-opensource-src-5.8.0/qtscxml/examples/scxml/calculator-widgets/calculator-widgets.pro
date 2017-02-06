QT += widgets scxml

CONFIG += c++11

STATECHARTS = ../calculator-common/statemachine.scxml

SOURCES += \
    calculator-widgets.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

# install
target.path = $$[QT_INSTALL_EXAMPLES]/scxml/calculator-widgets
INSTALLS += target
