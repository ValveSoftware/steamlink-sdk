TEMPLATE = app

QT += qml quick

target.path = $$[QT_INSTALL_EXAMPLES]/canvas3d/$$TARGET
INSTALLS += target

SOURCES += main.cpp

RESOURCES += qml.qrc

OTHER_FILES += doc/src/* \
               doc/images/*

EXAMPLE_FILES += \
    3dmodels.txt \
    readme.txt
