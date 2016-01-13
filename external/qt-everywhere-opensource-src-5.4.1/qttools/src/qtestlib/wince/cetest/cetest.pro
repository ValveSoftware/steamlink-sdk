CONFIG += console no_batch
CONFIG -= qt

DEFINES        += QT_BUILD_QMAKE QT_BOOTSTRAPPED QT_NO_CODECS QT_LITE_UNICODE QT_NO_LIBRARY \
                  QT_NO_STL QT_NO_COMPRESS QT_NO_DATASTREAM  \
                  QT_NO_TEXTCODEC QT_NO_UNICODETABLES QT_NO_THREAD \
                  QT_NO_SYSTEMLOCALE QT_NO_GEOM_VARIANT \
                  QT_NODLL QT_NO_QOBJECT

INCLUDEPATH = \
              $$QT.core.sources/../../tools/qtestlib/ce/cetest \
              $$QT.core.sources/../../qmake \
              $$QT.core.sources/../../tools/shared \
              $$QT.core.includes

VPATH += $$QT.core.sources/../../tools/shared

HEADERS += \
        remoteconnection.h \
        deployment.h

SOURCES += \
        remoteconnection.cpp \
        deployment.cpp \
        main.cpp

LIBS += ole32.lib advapi32.lib

isEmpty(QT_CE_RAPI_INC) {
    DEFINES += QT_CETEST_NO_ACTIVESYNC
    HEADERS += cetcpsyncconnection.h
    SOURCES += cetcpsyncconnection.cpp
} else {
    HEADERS += activesyncconnection.h
    SOURCES += activesyncconnection.cpp
    LIBS += rapi.lib
    INCLUDEPATH += $$QT_CE_RAPI_INC
    LIBS += -L$$QT_CE_RAPI_LIB
}

include(qmake_include.pri)
include(bootstrapped.pri)

load(qt_app)
