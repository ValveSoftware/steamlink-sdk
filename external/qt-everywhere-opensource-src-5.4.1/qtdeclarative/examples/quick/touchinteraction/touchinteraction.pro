TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += \
    touchinteraction.qrc \
    ../shared/shared.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/touchinteraction
INSTALLS += target
