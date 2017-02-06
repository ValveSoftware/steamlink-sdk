TEMPLATE = app

QT += qml quick

target.path = $$[QT_INSTALL_EXAMPLES]/canvas3d/$$TARGET
INSTALLS += target

SOURCES += main.cpp

RESOURCES += quickitemtexture.qrc

OTHER_FILES += qml/quickitemtexture/* \
               doc/src/* \
               doc/images/*

