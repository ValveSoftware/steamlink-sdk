TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += externaldraganddrop.qrc

EXAMPLE_FILES = \
    DragAndDropTextItem.qml

target.path = $$[QT_INSTALL_EXAMPLES]/quick/externaldraganddrop
INSTALLS += target
