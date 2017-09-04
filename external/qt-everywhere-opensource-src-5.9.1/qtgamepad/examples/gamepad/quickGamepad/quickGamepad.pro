TEMPLATE = app

QT += qml quick gamepad

SOURCES += main.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

target.path = $$[QT_INSTALL_EXAMPLES]/gamepad/quickGamepad
INSTALLS += target
