QT += widgets script
RESOURCES += helloscript.qrc
SOURCES += main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/script/helloscript
INSTALLS += target

maemo5: CONFIG += qt_example
