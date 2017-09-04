TEMPLATE = app
TARGET = flatstyle

QT += quick quickcontrols2 widgets

SOURCES += main.cpp

RESOURCES += \
   qtquickcontrols2.conf \
   $$files(*.qml) \
   $$files(imports/Theme/*) \
   $$files(Flat/*.qml)

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH = $$PWD/imports

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols2/flatstyle
INSTALLS += target
