QT += widgets declarative

HEADERS += piechart.h \
           pieslice.h
SOURCES += piechart.cpp \
           pieslice.cpp \
           main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/extending/chapter4-customPropertyTypes
qml.files = app.qml
qml.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/extending/chapter4-customPropertyTypes
INSTALLS += target qml
