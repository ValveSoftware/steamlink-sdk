TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += \
    customparticle.qrc \
    ../images.qrc \
    ../../shared/shared.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/particles/customparticle
INSTALLS += target

