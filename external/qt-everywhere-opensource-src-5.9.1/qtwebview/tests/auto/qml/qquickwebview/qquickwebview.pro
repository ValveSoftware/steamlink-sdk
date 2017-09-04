include(../tests.pri)

exists($${TARGET}.qrc):RESOURCES += $${TARGET}.qrc

HEADERS += ../shared/util.h
