requires(contains(QT_CONFIG, accessibility))

CXX_MODULE = qml
TARGET  = widgetsplugin
TARGETPATH = QtQuick/PrivateWidgets
IMPORT_VERSION = 1.1

SOURCES += \
    qquickqmessagebox.cpp \
    ../dialogs/qquickabstractmessagedialog.cpp \
    qquickqfiledialog.cpp \
    ../dialogs/qquickabstractfiledialog.cpp \
    qquickqcolordialog.cpp \
    ../dialogs/qquickabstractcolordialog.cpp \
    qquickqfontdialog.cpp \
    ../dialogs/qquickabstractfontdialog.cpp \
    ../dialogs/qquickabstractdialog.cpp \
    widgetsplugin.cpp

HEADERS += \
    qquickqmessagebox_p.h \
    qmessageboxhelper_p.h \
    ../dialogs/qquickabstractmessagedialog_p.h \
    qquickqfiledialog_p.h \
    ../dialogs/qquickabstractfiledialog_p.h \
    qquickqcolordialog_p.h \
    ../dialogs/qquickabstractcolordialog_p.h \
    qquickqfontdialog_p.h \
    ../dialogs/qquickabstractfontdialog_p.h \
    ../dialogs/qquickabstractdialog_p.h

QT += quick-private gui-private core-private qml-private  widgets

load(qml_plugin)
