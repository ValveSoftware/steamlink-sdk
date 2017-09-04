requires(contains(QT_CONFIG, accessibility))

CXX_MODULE = qml
TARGET  = widgetsplugin
TARGETPATH = QtQuick/PrivateWidgets
IMPORT_VERSION = 1.1

QT += quick-private gui-private core-private qml-private  widgets

SOURCES += \
    ../dialogs/qquickabstractdialog.cpp \
    widgetsplugin.cpp

HEADERS += \
    ../dialogs/qquickabstractdialog_p.h

qtConfig(messagebox) {
    HEADERS += \
        qquickqmessagebox_p.h \
        qmessageboxhelper_p.h \
        ../dialogs/qquickabstractmessagedialog_p.h

    SOURCES += \
        qquickqmessagebox.cpp \
        ../dialogs/qquickabstractmessagedialog.cpp
}

qtConfig(filedialog) {
    SOURCES += \
        qquickqfiledialog.cpp \
        ../dialogs/qquickabstractfiledialog.cpp

    HEADERS += \
        qquickqfiledialog_p.h \
        ../dialogs/qquickabstractfiledialog_p.h
}

qtConfig(colordialog) {
    SOURCES += \
        qquickqcolordialog.cpp \
        ../dialogs/qquickabstractcolordialog.cpp

    HEADERS += \
        qquickqcolordialog_p.h \
        ../dialogs/qquickabstractcolordialog_p.h
}

qtConfig(fontdialog) {
    SOURCES += \
        qquickqfontdialog.cpp \
        ../dialogs/qquickabstractfontdialog.cpp

    HEADERS += \
        qquickqfontdialog_p.h \
        ../dialogs/qquickabstractfontdialog_p.h
}

load(qml_plugin)
