TEMPLATE = app

QT += quick sensors

SOURCES += main.cpp \
        collector.cpp

HEADERS += collector.h

OTHER_FILES += qml/main.qml \
               qml/Button.qml
