QT += widgets declarative

HEADERS += piechart.h
SOURCES += piechart.cpp \
           main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/extending/chapter3-bindings
qml.files = app.qml
qml.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/extending/chapter3-bindings
INSTALLS += target qml
