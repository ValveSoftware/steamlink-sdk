TEMPLATE = app

DEFINES += QWEBKIT_EXAMPLE_NAME=\\\"youtubeview\\\"

QT += quick qml webkit
SOURCES += ../shared/main.cpp

mac: CONFIG -= app_bundle

target.path = $$[QT_INSTALL_EXAMPLES]/webkitqml/youtubeview
qml.files = youtubeview.qml content
qml.path = $$[QT_INSTALL_EXAMPLES]/webkitqml/youtubeview
INSTALLS += target qml

OTHER_FILES += \
    player.html

RESOURCES += youtubeview.qrc \
    ../shared/shared.qrc
