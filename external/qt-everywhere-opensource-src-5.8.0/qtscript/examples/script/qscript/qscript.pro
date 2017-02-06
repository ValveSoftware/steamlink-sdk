
QT += script
win32:CONFIG += console no_batch
mac:CONFIG -= app_bundle

SOURCES += main.cpp

include(../customclass/bytearrayclass.pri)

target.path = $$[QT_INSTALL_EXAMPLES]/script/qscript
INSTALLS += target

maemo5: CONFIG += qt_example
