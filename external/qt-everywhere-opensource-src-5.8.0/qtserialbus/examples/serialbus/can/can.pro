QT += serialbus widgets

TARGET = can
TEMPLATE = app

SOURCES += main.cpp \
    mainwindow.cpp \
    connectdialog.cpp

HEADERS += mainwindow.h \
    connectdialog.h

FORMS   += mainwindow.ui \
    connectdialog.ui

RESOURCES += can.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/serialbus/can
INSTALLS += target
