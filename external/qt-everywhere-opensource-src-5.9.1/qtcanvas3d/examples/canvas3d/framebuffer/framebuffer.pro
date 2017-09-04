TEMPLATE = app

QT += qml quick

target.path = $$[QT_INSTALL_EXAMPLES]/canvas3d/$$TARGET
INSTALLS += target

SOURCES += main.cpp

RESOURCES += framebuffer.qrc

OTHER_FILES += qml/framebuffer/* \
               doc/src/* \
               doc/images/*

