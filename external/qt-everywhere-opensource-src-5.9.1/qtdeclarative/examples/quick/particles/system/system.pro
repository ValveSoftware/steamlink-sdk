TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += \
    system.qrc \
    ../images.qrc \
    ../../shared/shared.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/particles/system
INSTALLS += target
