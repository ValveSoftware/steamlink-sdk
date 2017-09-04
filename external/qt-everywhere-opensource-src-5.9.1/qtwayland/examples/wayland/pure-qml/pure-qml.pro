QT += gui qml

SOURCES += \
    main.cpp

OTHER_FILES = \
    qml/main.qml \
    qml/Screen.qml \
    qml/Chrome.qml \
    qml/Keyboard.qml \
    images/background.jpg \

RESOURCES += pure-qml.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/wayland/pure-qml
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS pure-qml.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/wayland/pure-qml
INSTALLS += target sources
