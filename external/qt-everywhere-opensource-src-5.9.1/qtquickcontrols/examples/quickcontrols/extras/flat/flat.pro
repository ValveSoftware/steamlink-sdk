TEMPLATE = app
TARGET = flat
QT += quick

SOURCES += \
    main.cpp

RESOURCES += \
    flat.qrc

OTHER_FILES += \
    main.qml

DISTFILES += \
    Content.qml \
    SettingsIcon.qml

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols/extras/flat
INSTALLS += target
