QT += macextras

HEADERS += toolbarcontrollerwindow.h rasterwindow.h
SOURCES += toolbarcontrollerwindow.cpp rasterwindow.cpp main.cpp
RESOURCES += mactoolbar.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/macextras/mactoolbar
INSTALLS += target
