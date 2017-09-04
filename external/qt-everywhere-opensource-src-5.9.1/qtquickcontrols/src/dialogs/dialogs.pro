requires(contains(QT_CONFIG, accessibility))

CXX_MODULE = qml
TARGET  = dialogplugin
TARGETPATH = QtQuick/Dialogs
IMPORT_VERSION = 1.2

QMAKE_DOCS = $$PWD/doc/qtquickdialogs.qdocconf

SOURCES += \
    qquickabstractmessagedialog.cpp \
    qquickplatformmessagedialog.cpp \
    qquickmessagedialog.cpp \
    qquickabstractfiledialog.cpp \
    qquickplatformfiledialog.cpp \
    qquickfiledialog.cpp \
    qquickabstractcolordialog.cpp \
    qquickplatformcolordialog.cpp \
    qquickcolordialog.cpp \
    qquickabstractfontdialog.cpp \
    qquickplatformfontdialog.cpp \
    qquickfontdialog.cpp \
    qquickabstractdialog.cpp \
    qquickdialog.cpp \
    plugin.cpp

HEADERS += \
    qquickabstractmessagedialog_p.h \
    qquickplatformmessagedialog_p.h \
    qquickmessagedialog_p.h \
    qquickdialogassets_p.h \
    qquickabstractfiledialog_p.h \
    qquickplatformfiledialog_p.h \
    qquickfiledialog_p.h \
    qquickabstractcolordialog_p.h \
    qquickplatformcolordialog_p.h \
    qquickcolordialog_p.h \
    qquickabstractfontdialog_p.h \
    qquickplatformfontdialog_p.h \
    qquickfontdialog_p.h \
    qquickabstractdialog_p.h \
    qquickdialog_p.h

WIDGET_DIALOGS_QML_FILES = \
    WidgetMessageDialog.qml \
    WidgetFileDialog.qml \
    WidgetColorDialog.qml \
    WidgetFontDialog.qml

DIALOGS_QML_FILES += \
    DefaultMessageDialog.qml \
    DefaultFileDialog.qml \
    DefaultColorDialog.qml \
    DefaultFontDialog.qml \
    DefaultDialogWrapper.qml \
    qml/ColorSlider.qml \
    qml/DefaultWindowDecoration.qml \
    qml/IconButtonStyle.qml \
    qml/IconGlyph.qml \
    qml/qmldir \
    qml/icons.ttf \
    images/critical.png \
    images/information.png \
    images/question.png \
    images/warning.png \
    images/checkers.png \
    images/checkmark.png \
    images/copy.png \
    images/crosshairs.png \
    images/slider_handle.png \
    images/sunken_frame.png \
    images/window_border.png \
    $$WIDGET_DIALOGS_QML_FILES

ios|android|blackberry|winrt {
    DIALOGS_QML_FILES -= $$WIDGET_DIALOGS_QML_FILES
}

QT += quick-private gui gui-private core core-private qml qml-private

QML_FILES += $$DIALOGS_QML_FILES

qtquickcompiler {
    DEFINES += ALWAYS_LOAD_FROM_RESOURCES
    dialogs.files = $$QML_FILES
    dialogs.prefix = /QtQuick/Dialogs
    RESOURCES += dialogs
} else {
    !static: CONFIG += qmlcache
}

load(qml_plugin)
