TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += accessibility.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/quick-accessibility
INSTALLS += target

EXAMPLE_FILES += \
    accessibility.qmlproject
