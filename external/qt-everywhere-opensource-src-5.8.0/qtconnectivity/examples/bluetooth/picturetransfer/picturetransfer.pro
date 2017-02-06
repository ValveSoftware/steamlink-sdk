QT += quick bluetooth


TARGET = qml_picturetransfer
TEMPLATE = app

RESOURCES += \
    qmltransfer.qrc

OTHER_FILES += \
    bttransfer.qml \
    Button.qml \
    DeviceDiscovery.qml \
    PictureSelector.qml \
    FileSending.qml

HEADERS += \
    filetransfer.h

SOURCES += \
    filetransfer.cpp \
    main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/bluetooth/picturetransfer
INSTALLS += target

EXAMPLE_FILES += \
    icon.png
