TEMPLATE = app

QT += qml quick

target.path = $$[QT_INSTALL_EXAMPLES]/canvas3d/$$TARGET
INSTALLS += target

SOURCES += main.cpp

OTHER_FILES += qml/interaction/* \
               doc/src/* \
               doc/images/*

RESOURCES += interaction.qrc

EXAMPLE_FILES += \
    3dmodels.txt \
    readme.txt
