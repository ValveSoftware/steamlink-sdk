QT += quick-private positioning-private qml-private core-private

INCLUDEPATH *= $$PWD
DEFINES += TOUCH_EVENT_WORKAROUND

HEADERS += qdeclarativeposition_p.h \
           qdeclarativepositionsource_p.h \
           qdeclarativecoordinate_p.h \
           qdeclarativegeoshape.h \
           qdeclarativegeorectangle.h \
           qdeclarativegeocircle.h \
           locationvaluetypeprovider.h \
           locationsingleton.h \
           qdeclarativegeocoordinateanimation_p.h

SOURCES += qdeclarativeposition.cpp \
           positioning.cpp \
           qdeclarativepositionsource.cpp \
           qdeclarativecoordinate.cpp \
           qdeclarativegeoshape.cpp \
           qdeclarativegeorectangle.cpp \
           qdeclarativegeocircle.cpp \
           locationvaluetypeprovider.cpp \
           locationsingleton.cpp \
           qdeclarativegeocoordinateanimation.cpp

load(qml_plugin)

OTHER_FILES += \
    plugin.json \
    qmldir
