TEMPLATE = app
QT += widgets script
# Input
HEADERS += qcontext2dcanvas.h \
	   context2d.h \
	   domimage.h \
	   environment.h \
	   window.h
SOURCES += qcontext2dcanvas.cpp \
	   context2d.cpp \
	   domimage.cpp \
	   environment.cpp \
	   window.cpp \
	   main.cpp
RESOURCES += context2d.qrc

qtHaveModule(scripttools): QT += scripttools

target.path = $$[QT_INSTALL_EXAMPLES]/script/context2d
INSTALLS += target

maemo5: CONFIG += qt_example

