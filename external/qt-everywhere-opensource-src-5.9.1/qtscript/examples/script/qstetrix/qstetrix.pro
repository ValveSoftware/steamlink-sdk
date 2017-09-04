QT += widgets script uitools

HEADERS = tetrixboard.h
SOURCES = main.cpp \
	  tetrixboard.cpp

RESOURCES = tetrix.qrc

qtHaveModule(scripttools): QT += scripttools

target.path = $$[QT_INSTALL_EXAMPLES]/script/qstetrix
INSTALLS += target

maemo5: CONFIG += qt_example
