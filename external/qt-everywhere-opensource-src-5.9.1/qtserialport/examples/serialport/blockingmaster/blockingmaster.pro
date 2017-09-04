QT += widgets serialport

TARGET = blockingmaster
TEMPLATE = app

HEADERS += \
    dialog.h \
    masterthread.h

SOURCES += \
    main.cpp \
    dialog.cpp \
    masterthread.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/serialport/blockingmaster
INSTALLS += target
