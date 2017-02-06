TARGET = QtWebChannel
QT = core-private
CONFIG += warn_on strict_flags

QMAKE_DOCS = $$PWD/doc/qtwebchannel.qdocconf

RESOURCES += \
    resources.qrc

OTHER_FILES = \
    qwebchannel.js

PUBLIC_HEADERS += \
    qwebchannel.h \
    qwebchannelabstracttransport.h

PRIVATE_HEADERS += \
    qwebchannel_p.h \
    qmetaobjectpublisher_p.h \
    variantargument_p.h \
    signalhandler_p.h

SOURCES += \
    qwebchannel.cpp \
    qmetaobjectpublisher.cpp \
    qwebchannelabstracttransport.cpp

qtHaveModule(qml) {
    QT += qml

    SOURCES += \
        qqmlwebchannel.cpp \
        qqmlwebchannelattached.cpp

    PUBLIC_HEADERS += \
        qqmlwebchannel.h

    PRIVATE_HEADERS += \
        qqmlwebchannelattached_p.h
} else {
    DEFINES += QT_NO_JSVALUE
}

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS

load(qt_module)
