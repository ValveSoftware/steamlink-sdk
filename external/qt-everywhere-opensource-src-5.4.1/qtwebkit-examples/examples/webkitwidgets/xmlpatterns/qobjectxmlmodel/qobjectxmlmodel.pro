
FORMS       += forms/mainwindow.ui
QT +=       xmlpatterns webkitwidgets widgets
SOURCES     += qobjectxmlmodel.cpp main.cpp mainwindow.cpp xmlsyntaxhighlighter.cpp
HEADERS     += qobjectxmlmodel.h mainwindow.h xmlsyntaxhighlighter.h
RESOURCES   = queries.qrc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/webkitwidgets/xmlpatterns/qobjectxmlmodel
INSTALLS += target
