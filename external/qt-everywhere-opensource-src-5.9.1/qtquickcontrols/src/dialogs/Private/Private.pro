CXX_MODULE = qml
TARGET  = dialogsprivateplugin
TARGETPATH = QtQuick/Dialogs/Private
IMPORT_VERSION = 1.1

SOURCES += \
    qquickfontlistmodel.cpp \
    qquickwritingsystemlistmodel.cpp \
    dialogsprivateplugin.cpp

HEADERS += \
    qquickfontlistmodel_p.h \
    qquickwritingsystemlistmodel_p.h

QT += gui-private core-private qml

load(qml_plugin)
