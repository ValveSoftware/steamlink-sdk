TEMPLATE = app
TARGET = gallery
QT += quick quickcontrols2

SOURCES += \
    gallery.cpp

RESOURCES += \
    gallery.qml \
    qtquickcontrols2.conf \
    $$files(images/*.png) \
    $$files(images/+material/*.png) \
    $$files(pages/*.qml)

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols2/gallery
INSTALLS += target
