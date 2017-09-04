QT          += widgets xmlpatterns
FORMS       += schema.ui schema_mobiles.ui
HEADERS     = mainwindow.h ../shared/xmlsyntaxhighlighter.h
RESOURCES   = schema.qrc
SOURCES     = main.cpp mainwindow.cpp ../shared/xmlsyntaxhighlighter.cpp
INCLUDEPATH += ../shared/

target.path = $$[QT_INSTALL_EXAMPLES]/xmlpatterns/schema
INSTALLS += target

maemo5: CONFIG += qt_example

