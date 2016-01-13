TARGET = QtLocation
QT = core-private positioning-private

MODULE_PLUGIN_TYPES = \
    geoservices

QMAKE_DOCS = $$PWD/doc/qtlocation.qdocconf
OTHER_FILES += doc/src/*.qdoc   # show .qdoc files in Qt Creator

PUBLIC_HEADERS += \
                    qlocation.h \
                    qlocationglobal.h

SOURCES += \
           qlocation.cpp

include(maps/maps.pri)
include(places/places.pri)

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS

load(qt_module)
