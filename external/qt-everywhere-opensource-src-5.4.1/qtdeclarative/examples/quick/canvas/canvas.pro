TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += \
    canvas.qrc \
    ../shared/shared.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/canvas
INSTALLS += target
