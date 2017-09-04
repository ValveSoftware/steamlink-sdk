TEMPLATE = app

QT += qml quick

SOURCES += main.cpp

RESOURCES += multiwindowtest.qrc

OTHER_FILES += qml/multiwindowtest/*

DISTFILES += \
    qml/multiwindowtest/framebuffer.js \
    qml/multiwindowtest/quickitemtexture.js \
    qml/multiwindowtest/qtlogo.png \
    qml/multiwindowtest/framebuffer.qml \
    qml/multiwindowtest/quickitemtexture.qml

