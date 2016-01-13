QT = core script
win32: CONFIG += console
mac: CONFIG -= app_bundle

SOURCES += main.cpp

include(qsdbg.pri)

EXAMPLE_FILES = *.js

target.path = $$[QT_INSTALL_EXAMPLES]/script/qsdbg
INSTALLS += target

maemo5: CONFIG += qt_example
