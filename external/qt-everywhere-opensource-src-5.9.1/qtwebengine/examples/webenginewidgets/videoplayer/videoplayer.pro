TEMPLATE = app

QT += webenginewidgets

HEADERS += \
    mainwindow.h \
    fullscreenwindow.h \
    fullscreennotification.h

SOURCES += main.cpp \
    mainwindow.cpp \
    fullscreenwindow.cpp \
    fullscreennotification.cpp

RESOURCES += \
    data/videoplayer.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/webenginewidgets/videoplayer
INSTALLS += target
