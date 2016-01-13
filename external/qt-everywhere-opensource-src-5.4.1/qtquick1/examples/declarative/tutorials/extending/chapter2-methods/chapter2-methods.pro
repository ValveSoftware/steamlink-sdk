QT += widgets declarative

HEADERS += piechart.h
SOURCES += piechart.cpp \
           main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/extending/chapter2-methods
qml.files = app.qml
qml.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/extending/chapter2-methods
INSTALLS += target qml
