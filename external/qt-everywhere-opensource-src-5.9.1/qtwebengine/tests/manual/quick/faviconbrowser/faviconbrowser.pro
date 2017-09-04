QT += qml quick webengine
qtHaveModule(widgets) {
    QT += widgets # QApplication is required to get native styling with QtQuickControls
}

TARGET = faviconbrowser
TEMPLATE = app


SOURCES = \
    main.cpp

HEADERS = \
    utils.h

OTHER_FILES += \
    main.qml \
    AddressBar.qml \
    FaviconPanel.qml

RESOURCES += \
    faviconbrowser.qrc

