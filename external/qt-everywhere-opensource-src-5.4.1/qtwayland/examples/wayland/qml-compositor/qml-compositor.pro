DEFINES += QT_COMPOSITOR_QUICK

LIBS += -L ../../lib

QT += quick qml
QT += quick-private

QT += compositor

#  if you want to compile QtCompositor as part of the application
#  instead of linking to it, remove the QT += compositor and uncomment
#  the following line
#include (../../src/compositor/compositor.pri)

SOURCES += main.cpp

OTHER_FILES = ContrastEffect.qml  main.qml  WindowChrome.qml  WindowContainer.qml background.jpg closebutton.png compositor.js

target.path = $$[QT_INSTALL_EXAMPLES]/wayland/qml-compositor
sources.files = $$OTHER_FILES $$SOURCES $$HEADERS $$RESOURCES $$FORMS qml-compositor.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/wayland/qml-compositor
INSTALLS += target sources
