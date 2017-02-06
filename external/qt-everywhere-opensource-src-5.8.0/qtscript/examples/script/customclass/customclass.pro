QT = core script
win32: CONFIG += console
mac:CONFIG -= app_bundle

SOURCES += main.cpp

include(bytearrayclass.pri)

target.path = $$[QT_INSTALL_EXAMPLES]/script/customclass
INSTALLS += target

maemo5: CONFIG += qt_example
