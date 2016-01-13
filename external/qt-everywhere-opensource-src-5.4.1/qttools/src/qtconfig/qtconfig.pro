CONFIG   += x11
QT += gui-private widgets

SOURCES        += colorbutton.cpp main.cpp previewframe.cpp previewwidget.cpp mainwindow.cpp paletteeditoradvanced.cpp
HEADERS        += colorbutton.h previewframe.h previewwidget.h mainwindow.h paletteeditoradvanced.h

FORMS        = mainwindow.ui paletteeditoradvanced.ui previewwidget.ui
RESOURCES    = qtconfig.qrc

PROJECTNAME        = Qt Configuration

DBFILE          = qtconfig.db

load(qt_app)
