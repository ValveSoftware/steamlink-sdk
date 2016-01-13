TARGET = qicns

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QICNSPlugin
load(qt_plugin)

DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII

HEADERS += \
    qicnshandler_p.h

SOURCES += \
    main.cpp \
    qicnshandler.cpp

OTHER_FILES += icns.json
