QT += widgets declarative

HEADERS += piechart.h
SOURCES += piechart.cpp \
           main.cpp

qml.files = app.qml
qml.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/extending/chapter1-basics
target.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/extending/chapter1-basics
INSTALLS += target qml
