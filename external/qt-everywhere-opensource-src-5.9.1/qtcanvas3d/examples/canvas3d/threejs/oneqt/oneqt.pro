TEMPLATE = app

QT += qml quick

target.path = $$[QT_INSTALL_EXAMPLES]/canvas3d/threejs/$$TARGET
INSTALLS += target

SOURCES += main.cpp

OTHER_FILES += doc/src/* \
               doc/images/*

RESOURCES += oneqt.qrc

DISTFILES += \
    ImageCube.qml

ios {
    ios_icon.files = $$files($$PWD/ios/OneQtIcon*.png)
    QMAKE_BUNDLE_DATA += ios_icon
    QMAKE_INFO_PLIST = ios/Info.plist
}

EXAMPLE_FILES += \
    ios
