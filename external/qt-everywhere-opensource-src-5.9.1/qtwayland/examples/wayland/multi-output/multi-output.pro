QT += core gui qml

SOURCES += \
    main.cpp

OTHER_FILES = \
    qml/main.qml \
    qml/GridScreen.qml \
    qml/ShellScreen.qml \
    qml/ShellChrome.qml \
    images/background.jpg \

RESOURCES += multi-output.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/wayland/multi-output
INSTALLS += target
