TEMPLATE = app
TARGET = swipetoremove
QT += qml

SOURCES += \
    swipetoremove.cpp

RESOURCES += \
    swipetoremove.qml \
    fonts/fontello.ttf

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols2/swipetoremove
INSTALLS += target
