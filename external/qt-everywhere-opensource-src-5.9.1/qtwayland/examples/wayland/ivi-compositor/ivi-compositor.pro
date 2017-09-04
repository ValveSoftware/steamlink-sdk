QT += gui qml

SOURCES += \
    main.cpp

OTHER_FILES = \
    main.qml

RESOURCES += ivi-compositor.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/wayland/ivi-compositor
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS ivi-compositor.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/wayland/ivi-compositor
INSTALLS += target sources
