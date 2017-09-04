TARGET = bttestui
TEMPLATE = app

SOURCES += main.cpp \
    btlocaldevice.cpp

QT += quick bluetooth

android: QT += androidextras

OTHER_FILES += main.qml \
    Button.qml

RESOURCES += \
    bttest.qrc

HEADERS += \
    btlocaldevice.h
