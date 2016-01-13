CXX_MODULE = declarative
TARGET  = qmlwebkitplugin
TARGETPATH = QtWebKit

QT += declarative declarative-private widgets widgets-private gui gui-private core-private script-private webkitwidgets

SOURCES += qdeclarativewebview.cpp plugin.cpp
HEADERS += qdeclarativewebview_p.h

OTHER_FILES += plugin.json

load(qml1_plugin)
