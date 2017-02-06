QT += widgets serialport

TARGET = enumerator
TEMPLATE = app

SOURCES += \
    main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/serialport/enumerator
INSTALLS += target
