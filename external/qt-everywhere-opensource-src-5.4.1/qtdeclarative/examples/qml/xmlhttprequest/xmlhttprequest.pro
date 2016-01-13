TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += \
    xmlhttprequest.qrc \
    ../../quick/shared/quick_shared.qrc

EXAMPLE_FILES = \
    data.xml

target.path = $$[QT_INSTALL_EXAMPLES]/qml/xmlhttprequest
INSTALLS += target
