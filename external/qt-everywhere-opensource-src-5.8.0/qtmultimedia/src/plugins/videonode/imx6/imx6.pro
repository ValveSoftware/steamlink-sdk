TARGET = imx6vivantevideonode

QT += multimedia-private qtmultimediaquicktools-private

HEADERS += \
    qsgvivantevideonode.h \
    qsgvivantevideomaterialshader.h \
    qsgvivantevideomaterial.h \
    qsgvivantevideonodefactory.h

SOURCES += \
    qsgvivantevideonode.cpp \
    qsgvivantevideomaterialshader.cpp \
    qsgvivantevideomaterial.cpp \
    qsgvivantevideonodefactory.cpp

OTHER_FILES += \
    imx6.json

PLUGIN_TYPE = video/videonode
PLUGIN_EXTENDS = quick
PLUGIN_CLASS_NAME = QSGVivanteVideoNodeFactory
load(qt_plugin)
