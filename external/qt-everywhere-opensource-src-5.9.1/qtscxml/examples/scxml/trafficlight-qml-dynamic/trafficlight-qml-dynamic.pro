TEMPLATE = app

QT += qml scxml

SOURCES += trafficlight-qml-dynamic.cpp

RESOURCES += trafficlight-qml-dynamic.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/scxml/trafficlight-qml-dynamic
INSTALLS += target
