TARGET = Enginio
QT = core-private network
DEFINES += ENGINIOCLIENT_LIBRARY
MODULE = enginio

QMAKE_DOCS = $$PWD/doc/qtenginio.qdocconf
OTHER_FILES += \
    doc/qtenginio.qdocconf \
    doc/enginio_client.qdoc

include(../src.pri)

load(qt_module)

SOURCES += \
    enginiobackendconnection.cpp \
    enginioclient.cpp \
    enginioreply.cpp \
    enginiomodel.cpp \
    enginioidentity.cpp \
    enginiofakereply.cpp \
    enginiodummyreply.cpp \
    enginiostring.cpp

HEADERS += \
    chunkdevice_p.h \
    enginio.h \
    enginiobackendconnection_p.h \
    enginiobasemodel.h \
    enginiobasemodel_p.h \
    enginioclient.h\
    enginioclient_global.h \
    enginioclient_p.h \
    enginioreply.h \
    enginiomodel.h \
    enginioidentity.h \
    enginioobjectadaptor_p.h \
    enginioreply_p.h \
    enginiofakereply_p.h \
    enginiodummyreply_p.h \
    enginiostring_p.h \
    enginioclientconnection.h \
    enginiooauth2authentication.h \
    enginioreplystate.h


DEFINES +=  "ENGINIO_VERSION=\\\"$$MODULE_VERSION\\\""
