requires(qtConfig(accessibility))

TEMPLATE = app
TARGET = quicktestbrowser

macx: CONFIG -= app_bundle

HEADERS = utils.h
SOURCES = main.cpp

OTHER_FILES += ApplicationRoot.qml \
               BrowserDialog.qml \
               BrowserWindow.qml \
               ButtonWithMenu.qml \
               ContextMenuExtras.qml \
               DownloadView.qml \
               FeaturePermissionBar.qml \
               FullScreenNotification.qml

RESOURCES += resources.qrc

QT += qml quick webengine
CONFIG += c++11

qtHaveModule(widgets) {
    QT += widgets # QApplication is required to get native styling with QtQuickControls
}
