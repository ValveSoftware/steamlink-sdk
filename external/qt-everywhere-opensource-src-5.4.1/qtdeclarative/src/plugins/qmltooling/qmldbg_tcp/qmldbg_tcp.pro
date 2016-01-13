TARGET = qmldbg_tcp
QT = qml-private

PLUGIN_TYPE = qmltooling
PLUGIN_CLASS_NAME = QTcpServerConnection
load(qt_plugin)

include(qmldbg_tcp.pri)
