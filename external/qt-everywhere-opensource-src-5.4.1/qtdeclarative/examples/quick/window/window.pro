TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += \
    window.qrc \
    ../shared/shared.qrc
EXAMPLE_FILES = \
    window.qml

target.path = $$[QT_INSTALL_EXAMPLES]/quick/window
INSTALLS += target

ICON = resources/icon64.png
macx: ICON = resources/icon.icns
win32: RC_FILE = resources/window.rc
