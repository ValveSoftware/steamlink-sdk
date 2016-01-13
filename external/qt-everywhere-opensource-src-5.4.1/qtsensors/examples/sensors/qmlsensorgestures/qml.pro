TEMPLATE = app
TARGET = qmlsensorgestures
QT += quick
SOURCES = main.cpp

OTHER_FILES = \
    $$files(*.qml)

target.path = $$[QT_INSTALL_EXAMPLES]/sensors/qmlsensorgestures
INSTALLS += target

RESOURCES += \
    qml.qrc
