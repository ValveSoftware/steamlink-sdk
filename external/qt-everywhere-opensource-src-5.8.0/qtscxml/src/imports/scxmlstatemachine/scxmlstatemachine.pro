TARGET = scxml
TARGETPATH = QtScxml

QT = scxml qml-private core-private

SOURCES = \
    $$PWD/plugin.cpp \
    $$PWD/statemachineloader.cpp \
    $$PWD/eventconnection.cpp \
    $$PWD/statemachineextended.cpp \
    $$PWD/invokedservices.cpp

HEADERS = \
    $$PWD/eventconnection_p.h \
    $$PWD/invokedservices_p.h \
    $$PWD/statemachineextended_p.h \
    $$PWD/statemachineloader_p.h


load(qml_plugin)

OTHER_FILES += plugins.qmltypes qmldir
