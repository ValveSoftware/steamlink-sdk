QT += quick-private positioning-private qml-private core-private

INCLUDEPATH *= $$PWD

HEADERS += qdeclarativeposition_p.h \
           qdeclarativepositionsource_p.h \
           locationsingleton.h \
           qquickgeocoordinateanimation_p.h \
           qquickgeocoordinateanimation_p_p.h

SOURCES += qdeclarativeposition.cpp \
           positioning.cpp \
           qdeclarativepositionsource.cpp \
           locationsingleton.cpp \
           qquickgeocoordinateanimation.cpp

load(qml_plugin)

OTHER_FILES += \
    plugin.json \
    qmldir
