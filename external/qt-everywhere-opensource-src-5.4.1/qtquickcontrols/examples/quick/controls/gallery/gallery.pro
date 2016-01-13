TEMPLATE = app
TARGET = gallery

SOURCES += \
    main.cpp

RESOURCES += \
    gallery.qrc

OTHER_FILES += \
    main.qml \
    qml/ButtonPage.qml \
    qml/InputPage.qml \
    qml/ProgressPage.qml \
    qml/UI.js \
    qml/+android/UI.js \
    qml/+ios/UI.js \
    qml/+osx/UI.js

include(../shared/shared.pri)
