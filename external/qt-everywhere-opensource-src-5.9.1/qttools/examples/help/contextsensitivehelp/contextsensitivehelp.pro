TEMPLATE = app

QT     += help widgets
SOURCES += main.cpp \
           wateringconfigdialog.cpp \
           helpbrowser.cpp

HEADERS += wateringconfigdialog.h \
           helpbrowser.h

FORMS   += wateringconfigdialog.ui

target.path = $$[QT_INSTALL_EXAMPLES]/help/contextsensitivehelp
docs.files += $$PWD/docs
docs.path = $$target.path

INSTALLS += target docs
