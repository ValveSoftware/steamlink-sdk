TEMPLATE = app

QT += quick qml xml xmlpatterns
SOURCES += main.cpp
RESOURCES += rssnews.qrc

OTHER_FILES = rssnews.qml \
              content/*.qml \
              content/*.js \
              content/images/*

target.path = $$[QT_INSTALL_EXAMPLES]/quick/demos/rssnews
INSTALLS += target
