TARGET   = QtAxServer

QT += core-private gui-private widgets axbase
CONFIG  += static

DEFINES += QAX_SERVER

mingw: DEFINES += QT_NEEDS_QMAIN

HEADERS =   qaxaggregated.h \
            qaxbindable.h \
            qaxfactory.h \
            qclassfactory_p.h \
            ../shared/qaxtypes.h

SOURCES =   qaxaggregated.cpp \
            qaxserver.cpp \
            qaxserverbase.cpp \
            qaxbindable.cpp \
            qaxfactory.cpp \
            qaxservermain.cpp \
            qaxserverdll.cpp \
            qaxmain.cpp \
            ../shared/qaxtypes.cpp

MODULE = axserver
MODULE_DEFINES = QAXSERVER
MODULE_CONFIG = idcidl force_import_plugins
MODULE_MASTER_HEADER = ActiveQt
CONFIG  += no_module_headers
load(qt_module)
DEFINES -= QT_NO_CAST_TO_ASCII QT_USE_QSTRINGBUILDER
