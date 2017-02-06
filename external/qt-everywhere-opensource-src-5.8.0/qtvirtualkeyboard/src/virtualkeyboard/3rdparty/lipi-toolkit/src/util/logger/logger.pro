LIPILIBS = ltkutil
include(../../lipiplugin.pri)
CONFIG -= exceptions

HEADERS += \
    logger.h \

SOURCES += \
    logger.cpp \
    LTKLogger.cpp \

INCLUDEPATH += \
    ../lib \

win32 {
    DEFINES += LOGGER_EXPORTS
    LIBS += Advapi32.lib
    #DEF_FILE = logger.def
}
