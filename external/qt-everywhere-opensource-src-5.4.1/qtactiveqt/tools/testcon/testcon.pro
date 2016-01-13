TEMPLATE = app

CONFIG += qaxserver_no_postlink
QT += widgets axserver axcontainer printsupport

SOURCES  = main.cpp docuwindow.cpp mainwindow.cpp invokemethod.cpp changeproperties.cpp ambientproperties.cpp controlinfo.cpp
HEADERS  = docuwindow.h mainwindow.h invokemethod.h changeproperties.h ambientproperties.h controlinfo.h
FORMS    = mainwindow.ui invokemethod.ui changeproperties.ui ambientproperties.ui controlinfo.ui
RC_FILE  = testcon.rc

!mingw:QMAKE_POST_LINK = midl $$shell_quote($$shell_path($$PWD/testcon.idl)) && move testcon.tlb $(TARGETDIR)

target.path = $$[QT_INSTALL_BINS]
INSTALLS += target
