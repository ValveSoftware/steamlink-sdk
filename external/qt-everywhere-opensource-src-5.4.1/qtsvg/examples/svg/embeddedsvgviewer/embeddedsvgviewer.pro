TEMPLATE = app
QT += widgets svg

HEADERS += embeddedsvgviewer.h
SOURCES += embeddedsvgviewer.cpp main.cpp
RESOURCES += embeddedsvgviewer.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/svg/embeddedsvgviewer
INSTALLS += target

wince* {
   DEPLOYMENT_PLUGIN += qsvg
}

