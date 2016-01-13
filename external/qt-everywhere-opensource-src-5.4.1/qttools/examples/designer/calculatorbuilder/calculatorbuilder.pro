#! [0]
HEADERS     = calculatorform.h
RESOURCES   = calculatorbuilder.qrc
SOURCES     = calculatorform.cpp \
              main.cpp
QT += widgets uitools
#! [0]

target.path = $$[QT_INSTALL_EXAMPLES]/designer/calculatorbuilder
INSTALLS += target
