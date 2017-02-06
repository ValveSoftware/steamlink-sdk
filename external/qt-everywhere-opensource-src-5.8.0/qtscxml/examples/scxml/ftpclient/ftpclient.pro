QT = core scxml

TARGET = ftpclient

TEMPLATE = app
STATECHARTS += simpleftp.scxml

SOURCES += \
    main.cpp \
    ftpcontrolchannel.cpp \
    ftpdatachannel.cpp

HEADERS += \
    ftpcontrolchannel.h \
    ftpdatachannel.h

target.path = $$[QT_INSTALL_EXAMPLES]/scxml/ftpclient
INSTALLS += target
