TEMPLATE = app
TARGET = qmlqtsensors
QT += quick
SOURCES = main.cpp

RESOURCES += \
    qmlqtsensors.qrc

OTHER_FILES = \
    $$files(*.qml) \
    components

target.path = $$[QT_INSTALL_EXAMPLES]/sensors/qmlqtsensors
INSTALLS += target
