QT += gui qml

SOURCES += \
    main.cpp

OTHER_FILES = \
    qml/main.qml \

RESOURCES += multi-screen.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/wayland/multi-screen
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS multi-screen.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/wayland/multi-screen
INSTALLS += target sources

DISTFILES += \
    qml/Screen.qml \
    qml/Chrome.qml
