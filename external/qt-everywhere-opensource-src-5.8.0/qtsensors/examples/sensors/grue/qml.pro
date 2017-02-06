TEMPLATE = app
TARGET = grue_app
QT += quick

# Avoid going to release/debug subdirectory
win32: DESTDIR = ./

SOURCES = main.cpp

RESOURCES += \
    qml.qrc

OTHER_FILES = \
    $$files(*.qml) \
    grue.png

target.path = $$[QT_INSTALL_EXAMPLES]/sensors/grue
INSTALLS += target

EXAMPLE_FILES += \
    grue.xcf \
    icon.xcf
