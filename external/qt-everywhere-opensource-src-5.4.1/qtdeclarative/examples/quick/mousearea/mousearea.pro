TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += mousearea.qrc

EXAMPLE_FILES = \
    mousearea-wheel-example.qml

target.path = $$[QT_INSTALL_EXAMPLES]/quick/mousearea
INSTALLS += target
