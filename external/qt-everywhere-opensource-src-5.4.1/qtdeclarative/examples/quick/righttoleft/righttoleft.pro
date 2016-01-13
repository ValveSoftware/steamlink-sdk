TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += \
    righttoleft.qrc \
    ../shared/shared.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/righttoleft
INSTALLS += target
