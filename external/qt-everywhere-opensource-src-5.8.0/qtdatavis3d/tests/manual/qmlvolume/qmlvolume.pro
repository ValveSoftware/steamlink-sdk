!include( ../tests.pri ) {
    error( "Couldn't find the tests.pri file!" )
}

QT += datavisualization

# The .cpp file which was generated for your project. Feel free to hack it.
SOURCES += main.cpp \
           datasource.cpp
HEADERS += datasource.h

RESOURCES += qmlvolume.qrc

OTHER_FILES += doc/src/* \
               doc/images/* \
               qml/qmlvolume/*
