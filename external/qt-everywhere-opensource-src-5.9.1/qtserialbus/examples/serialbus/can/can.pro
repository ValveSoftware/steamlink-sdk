QT += serialbus widgets

TARGET = can
TEMPLATE = app

SOURCES += \
    bitratebox.cpp \
    connectdialog.cpp \
    main.cpp \
    mainwindow.cpp \

HEADERS += \
    bitratebox.h \
    connectdialog.h \
    mainwindow.h \

FORMS   += mainwindow.ui \
    connectdialog.ui

RESOURCES += can.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/serialbus/can
INSTALLS += target
