TEMPLATE = app
TARGET = geoflickr

QT += qml quick network positioning
SOURCES += qmllocationflickr.cpp

RESOURCES += \
    flickr.qrc

OTHER_FILES += flickr.qml \
               flickrcommon/* \
               flickrmobile/*

target.path = $$[QT_INSTALL_EXAMPLES]/positioning/geoflickr
INSTALLS += target
