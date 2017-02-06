QT += qml quick network nfc

SOURCES += \
    qmlposter.cpp

TARGET = qml_poster
TEMPLATE = app

RESOURCES += \
    poster.qrc

OTHER_FILES += \
    poster.qml

target.path = $$[QT_INSTALL_EXAMPLES]/nfc/poster
INSTALLS += target
