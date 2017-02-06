QT += widgets serialport

TARGET = blockingslave
TEMPLATE = app

HEADERS += \
    dialog.h \
    slavethread.h

SOURCES += \
    main.cpp \
    dialog.cpp \
    slavethread.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/serialport/blockingslave
INSTALLS += target
