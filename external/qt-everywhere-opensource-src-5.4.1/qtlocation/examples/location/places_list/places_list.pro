TARGET = places_list
TEMPLATE = app

QT += quick qml network
SOURCES = main.cpp

qmlcontent.files += $$files(*.qml)

target.path = $$[QT_INSTALL_EXAMPLES]/location/places_list
qmlcontent.path = $$[QT_INSTALL_EXAMPLES]/location/places_list

INSTALLS += target qmlcontent
