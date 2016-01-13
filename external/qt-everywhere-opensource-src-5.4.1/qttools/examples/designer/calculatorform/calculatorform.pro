#! [0]
HEADERS     = calculatorform.h
#! [0] #! [1]
FORMS       = calculatorform.ui
#! [1]
SOURCES     = calculatorform.cpp \
              main.cpp
QT += widgets

target.path = $$[QT_INSTALL_EXAMPLES]/designer/calculatorform
INSTALLS += target
