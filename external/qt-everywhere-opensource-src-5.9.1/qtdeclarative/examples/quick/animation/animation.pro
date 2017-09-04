TEMPLATE = app

QT += quick qml
SOURCES += main.cpp

RESOURCES += \
    ../shared/shared.qrc \
    animation.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/animation
INSTALLS += target
