CONFIG += testcase
SOURCES += tst_xmlpatternsxqts.cpp ../qxmlquery/TestFundament.cpp tst_suitetest.cpp

include(../xmlpatterns.pri)

HEADERS += tst_suitetest.h
LIBS += -L$$QT.xmlpatterns.libs -l$$XMLPATTERNS_SDK

# syncqt doesn't copy headers in tools/ so let's manually ensure
# it works with shadow builds and source builds.
INCLUDEPATH += $$(QTDIR)/include/QtXmlPatterns/private      \
               $$(QTSRCDIR)/include/QtXmlPatterns/private   \
               $$(QTSRCDIR)/tools/xmlpatterns               \
               $$(QTDIR)/tools/xmlpatterns                  \
               ../xmlpatternssdk/

QT += xml testlib
TARGET = tst_xmlpatternsxqts
requires(contains(QT_CONFIG,private_tests))

