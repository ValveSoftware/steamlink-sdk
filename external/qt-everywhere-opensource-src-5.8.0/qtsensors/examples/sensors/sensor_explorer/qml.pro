TEMPLATE = app
TARGET = sensor_explorer
QT += qml quick

qtHaveModule(widgets) {
    QT += widgets
}

# Avoid going to release/debug subdirectory
win32: DESTDIR = ./

SOURCES = main.cpp

OTHER_FILES = \
    sensor_explorer.qml

target.path = $$[QT_INSTALL_EXAMPLES]/sensors/sensor_explorer
INSTALLS += target

RESOURCES += \
    qml.qrc
