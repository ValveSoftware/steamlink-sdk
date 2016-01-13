QT = core qml

win32: CONFIG += console
mac:CONFIG -= app_bundle

SOURCES += main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/qml/shell
INSTALLS += target
