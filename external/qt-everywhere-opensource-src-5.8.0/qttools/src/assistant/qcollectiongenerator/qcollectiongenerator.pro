QT += network help-private
TARGET = qcollectiongenerator
SOURCES += ../shared/helpgenerator.cpp \
    main.cpp \
    ../shared/collectionconfiguration.cpp
HEADERS += ../shared/helpgenerator.h \
    ../shared/collectionconfiguration.h

load(qt_tool)
