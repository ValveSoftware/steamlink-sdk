TEMPLATE = app
TARGET = sidepanel
QT += quick

SOURCES += \
    sidepanel.cpp

RESOURCES += \
    sidepanel.qml \
    $$files(images/*.png)

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols2/sidepanel
INSTALLS += target
