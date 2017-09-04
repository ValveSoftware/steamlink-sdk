TEMPLATE = app

QT += qml quick

SOURCES += main.cpp

RESOURCES += \
    layouts.qrc
EXAMPLE_FILES = \
    layouts.qml

target.path = $$[QT_INSTALL_EXAMPLES]/quick/layouts
INSTALLS += target

