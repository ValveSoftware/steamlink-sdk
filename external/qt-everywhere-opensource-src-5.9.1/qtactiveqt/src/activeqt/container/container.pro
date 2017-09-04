TARGET   = QtAxContainer

QT += core-private gui gui-private widgets widgets-private axbase
CONFIG  += static

HEADERS =   ../control/qaxaggregated.h \
            qaxbase.h \
            qaxwidget.h \
            qaxobject.h \
            qaxscript.h \
            qaxselect.h \
            ../shared/qaxtypes.h \
            ../shared/qaxutils_p.h

SOURCES =   qaxbase.cpp \
            qaxdump.cpp \
            qaxwidget.cpp \
            qaxobject.cpp \
            qaxscript.cpp \
            qaxscriptwrapper.cpp \
            qaxselect.cpp \
            ../shared/qaxtypes.cpp \
            ../shared/qaxutils.cpp

FORMS =     qaxselect.ui

MODULE = axcontainer
MODULE_CONFIG = dumpcpp
MODULE_MASTER_HEADER = ActiveQt
CONFIG += no_module_headers
load(qt_module)
DEFINES -= QT_NO_CAST_TO_ASCII QT_USE_QSTRINGBUILDER
