#! [0]
SOURCES = calculatorform.cpp main.cpp
HEADERS = calculatorform.h
FORMS = calculatorform.ui
#! [0]

target.path = $$[QT_INSTALL_EXAMPLES]/uitools/multipleinheritance
INSTALLS += target

QT += widgets
