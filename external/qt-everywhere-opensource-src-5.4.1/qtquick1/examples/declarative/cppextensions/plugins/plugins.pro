TEMPLATE = lib
CONFIG += plugin
QT += widgets declarative

DESTDIR = org/qtproject/TimeExample
TARGET  = qmlqtimeexampleplugin

SOURCES += plugin.cpp

target.path += $$[QT_INSTALL_EXAMPLES]/declarative/cppextensions/plugins/org/qtproject/TimeExample

qdeclarativesources.files += \
    org/qtproject/TimeExample/qmldir \
    org/qtproject/TimeExample/center.png \
    org/qtproject/TimeExample/clock.png \
    org/qtproject/TimeExample/Clock.qml \
    org/qtproject/TimeExample/hour.png \
    org/qtproject/TimeExample/minute.png
qdeclarativesources.path += $$[QT_INSTALL_EXAMPLES]/declarative/cppextensions/plugins/org/qtproject/TimeExample

qml.files += plugins.qml
qml.path += $$[QT_INSTALL_EXAMPLES]/declarative/cppextensions/plugins

INSTALLS += target qdeclarativesources qml

