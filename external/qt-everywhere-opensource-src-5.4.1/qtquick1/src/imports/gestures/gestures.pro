CXX_MODULE = declarative
TARGET  = qmlgesturesplugin
TARGETPATH = Qt/labs/gestures

QT += declarative declarative-private widgets widgets-private gui gui-private core-private script-private

SOURCES += qdeclarativegesturearea.cpp plugin.cpp
HEADERS += qdeclarativegesturearea_p.h

OTHER_FILES += gestures.json

load(qml1_plugin)
