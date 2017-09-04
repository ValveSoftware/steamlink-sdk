TEMPLATE = app

QT += qml scxml
CONFIG += c++11

SOURCES += invoke-dynamic.cpp

RESOURCES += invoke-dynamic.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/scxml/invoke-dynamic
INSTALLS += target

