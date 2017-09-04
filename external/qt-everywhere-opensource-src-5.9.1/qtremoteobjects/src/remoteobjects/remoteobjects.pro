TARGET = QtRemoteObjects
MODULE = remoteobjects
MODULE_CONFIG = remoteobjects_repc
QT += network core-private
QT -= gui

QMAKE_DOCS = $$PWD/doc/qtremoteobjects.qdocconf
OTHER_FILES += $$PWD/doc/qtremoteobjects.qdocconf \
               $$PWD/doc/src/remoteobjects-cpp.qdoc \
               $$PWD/doc/src/remoteobjects-index.qdoc \
               $$PWD/doc/src/remoteobjects-overview.qdoc \
               $$PWD/doc/src/remoteobjects-repc.qdoc

load(qt_module)

DEFINES += QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII QT_NO_CAST_FROM_BYTEARRAY QT_NO_URL_CAST_FROM_STRING

INCLUDEPATH *= .

PUBLIC_HEADERS += \
    $$PWD/qconnectionfactories.h \
    $$PWD/qremoteobjectdynamicreplica.h \
    $$PWD/qremoteobjectsource.h \
    $$PWD/qremoteobjectreplica.h \
    $$PWD/qremoteobjectnode.h \
    $$PWD/qremoteobjectpendingcall.h \
    $$PWD/qtremoteobjectglobal.h \
    $$PWD/qremoteobjectregistry.h \
    $$PWD/qremoteobjectabstractitemmodeltypes.h \
    $$PWD/qremoteobjectabstractitemmodelreplica.h


PRIVATE_HEADERS += \
    $$PWD/qconnectionfactories_p.h \
    $$PWD/qconnection_local_backend_p.h \
    $$PWD/qconnection_tcpip_backend_p.h \
    $$PWD/qremoteobjectsourceio_p.h \
    $$PWD/qremoteobjectsource_p.h \
    $$PWD/qremoteobjectregistrysource_p.h \
    $$PWD/qremoteobjectnode_p.h \
    $$PWD/qremoteobjectpacket_p.h \
    $$PWD/qremoteobjectpendingcall_p.h \
    $$PWD/qremoteobjectreplica_p.h \
    $$PWD/qremoteobjectabstractitemmodelreplica_p.h \
    $$PWD/qremoteobjectabstractitemmodeladapter_p.h

SOURCES += \
    $$PWD/qconnection_local_backend.cpp \
    $$PWD/qconnection_tcpip_backend.cpp \
    $$PWD/qconnectionfactories.cpp \
    $$PWD/qremoteobjectdynamicreplica.cpp \
    $$PWD/qremoteobjectsource.cpp \
    $$PWD/qremoteobjectsourceio.cpp \
    $$PWD/qremoteobjectregistry.cpp \
    $$PWD/qremoteobjectregistrysource.cpp \
    $$PWD/qremoteobjectreplica.cpp \
    $$PWD/qremoteobjectnode.cpp \
    $$PWD/qremoteobjectpacket.cpp \
    $$PWD/qremoteobjectpendingcall.cpp \
    $$PWD/qtremoteobjectglobal.cpp \
    $$PWD/qremoteobjectabstractitemmodelreplica.cpp \
    $$PWD/qremoteobjectabstractitemmodeladapter.cpp

qnx {
    SOURCES += \
        qconnection_qnx_backend.cpp \
        qconnection_qnx_qiodevices.cpp \
        qconnection_qnx_server.cpp \

    PUBLIC_HEADERS += \
        qconnection_qnx_qiodevices.h \
        qconnection_qnx_server.h \

    PRIVATE_HEADERS +=\
        qconnection_qnx_backend_p.h \
        qconnection_qnx_qiodevices_p.h \
        qconnection_qnx_server_p.h \
        qconnection_qnx_global_p.h \

    contains(DEFINES , USE_HAM) {
      LIBS += -lham
      message(configured for using HAM)
    } else {
      message(configured without using HAM)
    }
}

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS

DEFINES += QT_BUILD_REMOTEOBJECTS_LIB

contains(QT_CONFIG, c++11): CONFIG += c++11
