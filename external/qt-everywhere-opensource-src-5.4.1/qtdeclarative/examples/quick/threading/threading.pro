TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += \
    threading.qrc \
    ../shared/shared.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/threading
INSTALLS += target
