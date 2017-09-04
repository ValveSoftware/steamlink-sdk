!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

SOURCE += main.cpp

QT += qml quick 3dcore 3drender 3dinput 3dquick 3dextras 3dquickextras

OTHER_FILES += *.qml

SOURCES += \
    main.cpp \
    abstractdeviceorientation.cpp \
    dummydeviceorientation.cpp

HEADERS += \
    abstractdeviceorientation.h \
    dummydeviceorientation.h

RESOURCES += \
    resources.qrc \
    ../exampleresources/obj.qrc

ios {
    QT += sensors sensors-private
    QMAKE_INFO_PLIST = Info.plist

    OBJECTIVE_SOURCES += \
        iosdeviceorientation.mm \
        iosdeviceorientation_p.mm

    HEADERS += \
        iosdeviceorientation.h \
        iosdeviceorientation_p.h
}
