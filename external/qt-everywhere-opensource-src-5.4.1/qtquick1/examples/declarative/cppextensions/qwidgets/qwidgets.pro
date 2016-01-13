TEMPLATE = lib
CONFIG += plugin
QT += widgets declarative

DESTDIR = QWidgets
TARGET = qmlqwidgetsplugin

SOURCES += qwidgets.cpp

qml.files = qwidgets.qml
qml.path += $$[QT_INSTALL_EXAMPLES]/declarative/cppextensions/qwidgets
qml2.files = QWidgets/qmldir
qml2.path += $$[QT_INSTALL_EXAMPLES]/declarative/cppextensions/plugins/QWidgets

target.path += $$[QT_INSTALL_EXAMPLES]/declarative/cppextensions/plugins

INSTALLS += target qml qml2

