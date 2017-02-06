TEMPLATE = app

QT += qml quick
qtHaveModule(multimedia): QT += multimedia
SOURCES += main.cpp
RESOURCES += maroon.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/demos/maroon
INSTALLS += target
