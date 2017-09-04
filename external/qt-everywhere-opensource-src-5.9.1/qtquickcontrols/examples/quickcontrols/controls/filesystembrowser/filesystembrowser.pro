TEMPLATE = app
TARGET = filesystembrowser
QT += qml quick widgets

SOURCES += main.cpp

RESOURCES += qml.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols/controls/filesystembrowser
INSTALLS += target
