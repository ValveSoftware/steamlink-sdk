TEMPLATE = app

QT += qml quick

target.path = $$[QT_INSTALL_EXAMPLES]/canvas3d/$$TARGET
INSTALLS += target

SOURCES += main.cpp

RESOURCES += textureandlight.qrc

OTHER_FILES += qml/textureandlight/* \
               doc/src/* \
               doc/images/*

