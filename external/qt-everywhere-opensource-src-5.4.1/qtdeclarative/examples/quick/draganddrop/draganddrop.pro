TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += \
    draganddrop.qrc \
    ../shared/shared.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/draganddrop
INSTALLS += target
