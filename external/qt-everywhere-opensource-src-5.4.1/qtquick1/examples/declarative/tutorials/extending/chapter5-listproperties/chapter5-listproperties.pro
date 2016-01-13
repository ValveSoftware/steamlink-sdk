QT += widgets declarative

HEADERS += piechart.h \
           pieslice.h
SOURCES += piechart.cpp \
           pieslice.cpp \
           main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/extending/chapter5-listproperties
qml.files = app.qml
qml.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/extending/chapter5-listproperties
INSTALLS += target qml
