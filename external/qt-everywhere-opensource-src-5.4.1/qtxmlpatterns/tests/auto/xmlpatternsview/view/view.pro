TARGET   = xmlpatternsview
TEMPLATE = app
DESTDIR  = $$QT.xmlpatterns.bins

include (../../xmlpatterns.pri)

# We add gui here, since xmlpatterns.pri pull it out.
QT      += xmlpatterns xml gui

LIBS += -l$$XMLPATTERNS_SDK

HEADERS = FunctionSignaturesView.h  \
          MainWindow.h              \
          TestCaseView.h            \
          TestResultView.h          \
          TreeSortFilter.h          \
          UserTestCase.h            \
          XDTItemItem.h

SOURCES = FunctionSignaturesView.cpp    \
          main.cpp                      \
          MainWindow.cpp                \
          TestCaseView.cpp              \
          TestResultView.cpp            \
          TreeSortFilter.cpp            \
          UserTestCase.cpp              \
          XDTItemItem.cpp

FORMS   = ui_BaseLinePage.ui            \
          ui_MainWindow.ui              \
          ui_TestCaseView.ui            \
          ui_TestResultView.ui          \
          ui_FunctionSignaturesView.ui

INCLUDEPATH += ../../xmlpatternsxqts/lib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
