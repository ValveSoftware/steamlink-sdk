TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += tweetsearch.qrc

OTHER_FILES = tweetsearch.qml \
              content/*.qml \
              content/*.js \
              content/resources/*

target.path = $$[QT_INSTALL_EXAMPLES]/quick/demos/tweetsearch
INSTALLS += target
