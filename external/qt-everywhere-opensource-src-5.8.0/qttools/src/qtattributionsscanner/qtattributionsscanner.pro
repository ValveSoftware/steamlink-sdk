CONFIG += console

SOURCES += \
    jsongenerator.cpp \
    main.cpp \
    packagefilter.cpp \
    qdocgenerator.cpp \
    scanner.cpp

HEADERS += \
    jsongenerator.h \
    logging.h \
    package.h \
    packagefilter.h \
    qdocgenerator.h \
    scanner.h

DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII

option(host_build)
load(qt_tool)
