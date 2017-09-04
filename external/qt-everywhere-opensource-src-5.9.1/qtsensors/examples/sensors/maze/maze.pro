TEMPLATE = app
TARGET = maze
QT += quick sensors
SOURCES = main.cpp

OTHER_FILES = \
    components \
    $$files(*.qml) \
    $$files(*.js) \
    content \
    android/AndroidManifest.xml

target.path = $$[QT_INSTALL_EXAMPLES]/sensors/maze
INSTALLS += target

RESOURCES += \
    maze.qrc

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
