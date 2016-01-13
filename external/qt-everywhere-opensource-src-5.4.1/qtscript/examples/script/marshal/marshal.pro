QT = core script
CONFIG += console
SOURCES += main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/script/marshal
INSTALLS += target

maemo5: CONFIG += qt_example
