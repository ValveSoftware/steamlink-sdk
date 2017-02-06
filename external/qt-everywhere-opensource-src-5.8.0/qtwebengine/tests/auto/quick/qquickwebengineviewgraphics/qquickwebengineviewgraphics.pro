include(../tests.pri)
CONFIG -= testcase      # remove, once this passes in the CI
exists($${TARGET}.qrc):RESOURCES += $${TARGET}.qrc
QT_PRIVATE += webengine-private
