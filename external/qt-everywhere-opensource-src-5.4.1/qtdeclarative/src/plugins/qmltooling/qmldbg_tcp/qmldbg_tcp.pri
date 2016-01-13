QT += network core-private

SOURCES += \
    $$PWD/qtcpserverconnection.cpp \
    $$PWD/../shared/qpacketprotocol.cpp

HEADERS += \
    $$PWD/qtcpserverconnection.h \
    $$PWD/../shared/qpacketprotocol.h

INCLUDEPATH += $$PWD \
    $$PWD/../shared

OTHER_FILES += $$PWD/qtcpserverconnection.json
