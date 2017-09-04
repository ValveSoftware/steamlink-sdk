QT = core
QT += serialport

CONFIG += console
CONFIG -= app_bundle

TARGET = cwriterasync
TEMPLATE = app

HEADERS += \
    serialportwriter.h

SOURCES += \
    main.cpp \
    serialportwriter.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/serialport/cwriterasync
INSTALLS += target
