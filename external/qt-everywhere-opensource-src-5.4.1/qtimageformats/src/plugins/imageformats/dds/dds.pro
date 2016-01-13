TARGET = qdds

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QDDSPlugin
load(qt_plugin)

HEADERS += \
    ddsheader.h \
    qddshandler.h

SOURCES += \
    ddsheader.cpp \
    main.cpp \
    qddshandler.cpp

OTHER_FILES += dds.json
