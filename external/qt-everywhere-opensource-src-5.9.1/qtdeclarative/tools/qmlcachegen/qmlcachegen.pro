option(host_build)

QT = qmldevtools-private
DEFINES += QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII

SOURCES = qmlcachegen.cpp
TARGET = qmlcachegen

build_integration.files = qmlcache.prf
build_integration.path = $$[QT_HOST_DATA]/mkspecs/features
prefix_build: INSTALLS += build_integration
else: COPIES += build_integration

load(qt_tool)
