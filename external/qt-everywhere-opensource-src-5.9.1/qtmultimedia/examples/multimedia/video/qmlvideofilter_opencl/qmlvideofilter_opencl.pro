TEMPLATE = app
TARGET = qmlvideofilter_opencl

QT += quick multimedia

SOURCES = main.cpp
HEADERS = rgbframehelper.h

RESOURCES = qmlvideofilter_opencl.qrc
OTHER_FILES = main.qml

target.path = $$[QT_INSTALL_EXAMPLES]/multimedia/video/qmlvideofilter_opencl
INSTALLS += target

osx: LIBS += -framework OpenCL
unix: !osx: LIBS += -lOpenCL
win32:!winrt: LIBS += -lopengl32 -lOpenCL
