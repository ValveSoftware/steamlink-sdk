option(host_build)
CONFIG += force_bootstrap

DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII WINRT_LIBRARY

SOURCES += \
    main.cpp \
    runner.cpp \
    appxengine.cpp \
    appxlocalengine.cpp \
    appxphoneengine.cpp

HEADERS += \
    runner.h \
    runnerengine.h \
    appxengine.h \
    appxengine_p.h \
    appxlocalengine.h \
    appxphoneengine.h

LIBS += -lruntimeobject -lwsclient -lShlwapi -lurlmon -lxmllite -lcrypt32

include(../shared/corecon/corecon.pri)

load(qt_tool)
