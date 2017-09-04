TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += \
    text.qrc \
    ../shared/shared.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/text
INSTALLS += target
