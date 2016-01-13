TARGET = customgeometry
QT += quick

SOURCES += \
    main.cpp \
    beziercurve.cpp

HEADERS += \
    beziercurve.h

RESOURCES += customgeometry.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/scenegraph/customgeometry
INSTALLS += target
