HEADERS         = textfinder.h
RESOURCES       = textfinder.qrc
SOURCES         = textfinder.cpp main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/uitools/textfinder
INSTALLS += target

QT += widgets uitools
