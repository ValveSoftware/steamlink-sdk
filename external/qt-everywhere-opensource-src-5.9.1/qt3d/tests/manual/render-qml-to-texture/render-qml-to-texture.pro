!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dextras 3dcore 3drender 3dinput 3dquick qml quick 3dquickrender 3dquickscene2d

SOURCES += main.cpp \
    planematerial.cpp

RESOURCES += \
    render-qml-to-texture.qrc

OTHER_FILES += \
    main.qml

DISTFILES += \
    OffscreenGui.qml \
    TextRectangle.qml

HEADERS += \
    planematerial.h
