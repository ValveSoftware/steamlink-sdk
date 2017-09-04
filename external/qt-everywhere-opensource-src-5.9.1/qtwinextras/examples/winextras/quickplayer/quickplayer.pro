TEMPLATE = app
TARGET = quickplayer

QT += quick

SOURCES = \
    main.cpp

OTHER_FILES += \
    qml/main.qml

RESOURCES += \
    quickplayer.qrc

RC_ICONS = images/qt-logo.ico

target.path = $$[QT_INSTALL_EXAMPLES]/winextras/quickplayer
INSTALLS += target
