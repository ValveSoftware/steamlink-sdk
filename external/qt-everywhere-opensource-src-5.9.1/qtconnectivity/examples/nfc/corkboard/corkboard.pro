QT += quick nfc

SOURCES += \
    main.cpp

TARGET = qml_corkboard
TEMPLATE = app

RESOURCES += \
    corkboard.qrc

OTHER_FILES += \
    corkboards.qml \
    Mode.qml

!android-embedded {
ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
}

target.path = $$[QT_INSTALL_EXAMPLES]/nfc/corkboard
INSTALLS += target

EXAMPLE_FILES += \
    icon.png
