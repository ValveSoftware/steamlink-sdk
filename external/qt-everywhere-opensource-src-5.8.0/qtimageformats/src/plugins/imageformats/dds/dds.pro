TARGET = qdds

HEADERS += \
    ddsheader.h \
    qddshandler.h

SOURCES += \
    ddsheader.cpp \
    main.cpp \
    qddshandler.cpp

OTHER_FILES += dds.json

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QDDSPlugin
load(qt_plugin)
