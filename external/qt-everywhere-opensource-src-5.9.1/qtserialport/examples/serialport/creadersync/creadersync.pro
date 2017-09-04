QT = core
QT += serialport

CONFIG += console
CONFIG -= app_bundle

TARGET = creadersync
TEMPLATE = app

SOURCES += \
    main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/serialport/creadersync
INSTALLS += target
