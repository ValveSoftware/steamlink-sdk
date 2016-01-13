TARGET = places_map
TEMPLATE = app

QT += quick qml network
SOURCES = main.cpp

qmlcontent.files += $$files(*.qml) \
                    marker.png

target.path = $$[QT_INSTALL_EXAMPLES]/location/places_map
qmlcontent.path = $$[QT_INSTALL_EXAMPLES]/location/places_map

INSTALLS += target qmlcontent
