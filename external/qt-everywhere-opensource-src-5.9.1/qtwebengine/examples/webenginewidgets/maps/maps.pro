TEMPLATE = app

QT += webenginewidgets

HEADERS += \
    mainwindow.h

SOURCES += main.cpp \
    mainwindow.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/webenginewidgets/maps
INSTALLS += target
