TEMPLATE = app
TARGET = styles
QT += qml quick

SOURCES += \
    main.cpp
RESOURCES += \
    styles.qrc
OTHER_FILES += \
    main.qml

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols/controls/styles
INSTALLS += target
