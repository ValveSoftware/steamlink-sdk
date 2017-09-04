QT += qml quick

HEADERS += xorblender.h
SOURCES += xorblender.cpp main.cpp

RESOURCES += twotextureproviders.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/scenegraph/twotextureproviders
INSTALLS += target

OTHER_FILES += \
    main.qml
