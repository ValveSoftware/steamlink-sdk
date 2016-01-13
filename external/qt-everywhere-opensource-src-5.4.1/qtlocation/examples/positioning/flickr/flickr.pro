TEMPLATE=app
TARGET = qml_position_flickr

QT += qml quick network positioning
SOURCES += qmllocationflickr.cpp

RESOURCES += \
    flickr.qrc

OTHER_FILES += flickr.qml \
               flickrcommon/* \
               flickrmobile/*

target.path = $$[QT_INSTALL_EXAMPLES]/positioning/flickr
INSTALLS += target
