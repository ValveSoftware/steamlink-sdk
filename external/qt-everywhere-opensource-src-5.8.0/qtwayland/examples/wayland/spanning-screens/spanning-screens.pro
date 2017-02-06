QT += gui qml

SOURCES += \
    main.cpp

OTHER_FILES = \
    main.qml

RESOURCES += spanning-screens.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/wayland/spanning-screens
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS spanning-screens.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/wayland/spanning-screens
INSTALLS += target sources
