QT += gui qml

SOURCES += \
    main.cpp

OTHER_FILES = \
    main.qml

RESOURCES += minimal-qml.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/wayland/minimal-qml
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS minimal-qml.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/wayland/minimal-qml
INSTALLS += target sources
