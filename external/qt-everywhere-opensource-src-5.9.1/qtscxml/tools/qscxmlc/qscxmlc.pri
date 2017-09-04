DEFINES += BUILD_QSCXMLC

SOURCES += \
    $$PWD/generator.cpp \
    $$PWD/qscxmlc.cpp \
    $$PWD/scxmlcppdumper.cpp

HEADERS += \
    $$PWD/moc.h \
    $$PWD/generator.h \
    $$PWD/outputrevision.h \
    $$PWD/qscxmlc.h \
    $$PWD/utils.h \
    $$PWD/scxmlcppdumper.h

HEADERS += \
    $$PWD/../../src/scxml/qscxmlcompiler.h \
    $$PWD/../../src/scxml/qscxmlcompiler_p.h \
    $$PWD/../../src/scxml/qscxmlglobals.h \
    $$PWD/../../src/scxml/qscxmlexecutablecontent.h \
    $$PWD/../../src/scxml/qscxmlexecutablecontent_p.h \
    $$PWD/../../src/scxml/qscxmlerror.h \
    $$PWD/../../src/scxml/qscxmltabledata.h

SOURCES += \
    $$PWD/../../src/scxml/qscxmlcompiler.cpp \
    $$PWD/../../src/scxml/qscxmlexecutablecontent.cpp \
    $$PWD/../../src/scxml/qscxmlerror.cpp \
    $$PWD/../../src/scxml/qscxmltabledata.cpp

DEFINES += QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII
INCLUDEPATH *= $$QT.scxml.includes $$QT.scxml_private.includes
