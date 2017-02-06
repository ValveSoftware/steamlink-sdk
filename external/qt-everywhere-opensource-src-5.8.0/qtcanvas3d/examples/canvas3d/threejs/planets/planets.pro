TEMPLATE = app

QT += qml quick

target.path = $$[QT_INSTALL_EXAMPLES]/canvas3d/threejs/$$TARGET
INSTALLS += target

SOURCES += main.cpp

OTHER_FILES += *.qml \
               planets.js \
               doc/src/* \
               doc/images/*

RESOURCES += planets.qrc

ios {
    ios_icon.files = $$files($$PWD/ios/AppIcon*.png)
    QMAKE_BUNDLE_DATA += ios_icon
    QMAKE_INFO_PLIST = ios/Info.plist
}

EXAMPLE_FILES += \
    ios
