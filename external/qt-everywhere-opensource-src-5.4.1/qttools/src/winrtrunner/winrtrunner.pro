option(host_build)
CONFIG += force_bootstrap

DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII WINRT_LIBRARY

SOURCES += main.cpp runner.cpp
HEADERS += runner.h runnerengine.h

DEFINES += RTRUNNER_NO_APPXLOCAL RTRUNNER_NO_APPXPHONE RTRUNNER_NO_XAP

win32-msvc2012|win32-msvc2013 {
    SOURCES += appxengine.cpp appxlocalengine.cpp
    HEADERS += appxengine.h appxengine_p.h appxlocalengine.h
    LIBS += -lruntimeobject -lwsclient -lShlwapi
    DEFINES -= RTRUNNER_NO_APPXLOCAL

    include(../shared/corecon/corecon.pri)
    SOURCES += xapengine.cpp
    HEADERS += xapengine.h
    DEFINES -= RTRUNNER_NO_XAP

    win32-msvc2013 {
        LIBS += -lurlmon -lxmllite
        SOURCES += appxphoneengine.cpp
        HEADERS += appxphoneengine.h
        DEFINES -= RTRUNNER_NO_APPXPHONE
    }

    # Use zip class from qtbase
    SOURCES += \
        qzip/qzip.cpp
    HEADERS += \
        qzip/qzipreader_p.h \
        qzip/qzipwriter_p.h
    INCLUDEPATH += qzip $$[QT_INSTALL_HEADERS/get]/QtZlib
}

load(qt_tool)
