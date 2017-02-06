TEMPLATE = app

OBJECTIVE_SOURCES += main.mm
HEADERS += window.h
SOURCES += window.cpp
LIBS += -framework Cocoa

QT += gui widgets macextras
QT += widgets-private gui-private core-private

target.path = $$[QT_INSTALL_EXAMPLES]/macextras/embeddedqwindow
INSTALLS += target
