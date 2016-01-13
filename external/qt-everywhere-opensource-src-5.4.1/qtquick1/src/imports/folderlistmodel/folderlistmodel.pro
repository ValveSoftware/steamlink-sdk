CXX_MODULE = declarative
TARGET  = qmlfolderlistmodelplugin
TARGETPATH = Qt/labs/folderlistmodel

QT += widgets declarative script

SOURCES += qdeclarativefolderlistmodel.cpp plugin.cpp
HEADERS += qdeclarativefolderlistmodel.h

OTHER_FILES += folderlistmodel.json

load(qml1_plugin)
