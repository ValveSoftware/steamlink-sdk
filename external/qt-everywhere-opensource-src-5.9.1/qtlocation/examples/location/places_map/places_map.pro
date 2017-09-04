TARGET = places_map
TEMPLATE = app

QT += quick qml network positioning location
SOURCES = main.cpp

RESOURCES += \
    places_map.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/location/places_map
INSTALLS += target

