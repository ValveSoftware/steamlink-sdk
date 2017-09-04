include (../xmlpatterns.pri)

TARGET   = $$XMLPATTERNS_SDK
TEMPLATE = lib
DEFINES += Q_PATTERNISTSDK_BUILDING QT_ENABLE_QEXPLICITLYSHAREDDATAPOINTER_STATICCAST

CONFIG += exceptions

# lib_bundle ensures we get a framework on OS X, a library bundle.
CONFIG  += resources

mac {
    CONFIG += absolute_library_soname
    target.path=$$[QT_INSTALL_LIBS]
    INSTALLS += target
}

# We add gui, because xmlpatterns.pri pull it out.
QT      += xmlpatterns xml network testlib gui

DESTDIR    = $$QT.xmlpatterns.libs
DLLDESTDIR = $$QT.xmlpatterns.bins

# syncqt doesn't copy headers in tools/ so let's manually ensure
# it works with shadow builds and source builds.
INCLUDEPATH += $$QT.xmlpatterns.private_includes \
               ../../../tools/xmlpatterns

HEADERS = ASTItem.h                     \
          DebugExpressionFactory.h      \
          ErrorHandler.h                \
          ErrorItem.h                   \
          ExitCode.h                    \
          ExpressionInfo.h              \
          ExpressionNamer.h             \
          ExternalSourceLoader.h        \
          Global.h                      \
          ResultThreader.h              \
          TestBaseLine.h                \
          TestCase.h                    \
          TestContainer.h               \
          TestGroup.h                   \
          TestItem.h                    \
          TestResult.h                  \
          TestResultHandler.h           \
          TestSuite.h                   \
          TestSuiteHandler.h            \
          TestSuiteResult.h             \
          TreeItem.h                    \
          TreeModel.h                   \
          Worker.h                      \
          XMLWriter.h                   \
          XQTSTestCase.h                \
          XSDTestSuiteHandler.h         \
          XSDTSTestCase.h               \
          XSLTTestSuiteHandler.h

SOURCES = ASTItem.cpp                   \
          DebugExpressionFactory.cpp    \
          ErrorHandler.cpp              \
          ErrorItem.cpp                 \
          ExpressionInfo.cpp            \
          ExpressionNamer.cpp           \
          ExternalSourceLoader.cpp      \
          Global.cpp                    \
          ResultThreader.cpp            \
          TestBaseLine.cpp              \
          TestCase.cpp                  \
          TestContainer.cpp             \
          TestGroup.cpp                 \
          TestResult.cpp                \
          TestResultHandler.cpp         \
          TestSuite.cpp                 \
          TestSuiteHandler.cpp          \
          TestSuiteResult.cpp           \
          TreeItem.cpp                  \
          TreeModel.cpp                 \
          Worker.cpp                    \
          XMLWriter.cpp                 \
          XQTSTestCase.cpp              \
          XSDTestSuiteHandler.cpp       \
          XSDTSTestCase.cpp             \
          XSLTTestSuiteHandler.cpp
