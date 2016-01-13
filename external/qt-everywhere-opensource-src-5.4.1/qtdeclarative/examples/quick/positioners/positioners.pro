TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += \
    positioners.qrc \
    ../shared/shared.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/positioners
INSTALLS += target
