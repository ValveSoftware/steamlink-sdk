QT += qml quick multimedia

SOURCES += main.cpp
RESOURCES += declarative-radio.qrc

EXAMPLE_FILES += view.qml

target.path = $$[QT_INSTALL_EXAMPLES]/multimedia/declarative-radio
INSTALLS += target
