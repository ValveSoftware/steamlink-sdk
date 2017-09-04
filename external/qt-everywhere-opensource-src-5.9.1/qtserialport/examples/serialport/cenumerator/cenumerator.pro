QT = core
QT += serialport

CONFIG += console
CONFIG -= app_bundle

TARGET = cenumerator
TEMPLATE = app

SOURCES += \
    main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/serialport/cenumerator
INSTALLS += target
