QT = core
QT += serialport

CONFIG += console
CONFIG -= app_bundle

TARGET = cwritersync
TEMPLATE = app

SOURCES += \
    main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/serialport/cwritersync
INSTALLS += target
