TARGET = qtsgvideonode_android
QT += quick multimedia-private qtmultimediaquicktools-private

PLUGIN_TYPE = video/videonode
PLUGIN_EXTENDS = quick
PLUGIN_CLASS_NAME = QAndroidSGVideoNodeFactoryPlugin
load(qt_plugin)

HEADERS += \
    qandroidsgvideonodeplugin.h \
    qandroidsgvideonode.h

SOURCES += \
    qandroidsgvideonodeplugin.cpp \
    qandroidsgvideonode.cpp

OTHER_FILES += android_videonode.json
