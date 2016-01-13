
INCLUDEPATH *= $$PWD

DEFINES += PROEVALUATOR_CUMULATIVE PROEVALUATOR_INIT_PROPS

HEADERS += \
    $$PWD/qmake_global.h \
    $$PWD/ioutils.h \
    $$PWD/qmakevfs.h \
    $$PWD/proitems.h \
    $$PWD/qmakeglobals.h \
    $$PWD/qmakeparser.h \
    $$PWD/qmakeevaluator.h \
    $$PWD/qmakeevaluator_p.h \
    $$PWD/profileevaluator.h

SOURCES += \
    $$PWD/ioutils.cpp \
    $$PWD/qmakevfs.cpp \
    $$PWD/proitems.cpp \
    $$PWD/qmakeglobals.cpp \
    $$PWD/qmakeparser.cpp \
    $$PWD/qmakeevaluator.cpp \
    $$PWD/qmakebuiltins.cpp \
    $$PWD/profileevaluator.cpp
