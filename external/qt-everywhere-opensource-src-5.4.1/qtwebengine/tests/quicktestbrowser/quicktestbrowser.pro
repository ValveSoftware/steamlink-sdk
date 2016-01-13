TEMPLATE = app
TARGET = quicktestbrowser

macx: CONFIG -= app_bundle

HEADERS = quickwindow.h \
          util.h
SOURCES = quickwindow.cpp \
          main.cpp

OTHER_FILES += ButtonWithMenu.qml \
               ContextMenuExtras.qml \
               FeaturePermissionBar.qml \
               quickwindow.qml

RESOURCES += resources.qrc

QT += qml quick webengine

qtHaveModule(widgets) {
    QT += widgets # QApplication is required to get native styling with QtQuickControls
}
