QT += gui webchannel widgets websockets

CONFIG += warn_on

SOURCES += \
    main.cpp \
    ../shared/websockettransport.cpp \
    ../shared/websocketclientwrapper.cpp

HEADERS += \
    ../shared/websockettransport.h \
    ../shared/websocketclientwrapper.h

FORMS += \
    dialog.ui

DEFINES += "BUILD_DIR=\"\\\""$$OUT_PWD"\\\"\""

exampleassets.files += \
    index.html
exampleassets.path = $$[QT_INSTALL_EXAMPLES]/webchannel/standalone
include(../exampleassets.pri)

target.path = $$[QT_INSTALL_EXAMPLES]/webchannel/standalone
INSTALLS += target
