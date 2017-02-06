QT += qml quick

HEADERS += piechart.h
SOURCES += piechart.cpp \
           main.cpp

RESOURCES += chapter3-bindings.qrc

DESTPATH = $$[QT_INSTALL_EXAMPLES]/qml/tutorials/extending-qml/chapter3-bindings
target.path = $$DESTPATH

qml.files = *.qml
qml.path = $$DESTPATH

INSTALLS += target qml
