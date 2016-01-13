TEMPLATE = lib
CONFIG += plugin
QT += declarative widgets

DESTDIR = lib
OBJECTS_DIR = tmp
MOC_DIR = tmp

HEADERS += piechart.h \
           pieslice.h \
           chartsplugin.h

SOURCES += piechart.cpp \
           pieslice.cpp \
           chartsplugin.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/extending/chapter6-plugins
qml.files = app.qml qmldir
qml.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/extending/chapter6-plugins
INSTALLS += target qml
