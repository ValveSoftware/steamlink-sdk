TEMPLATE = app
TARGET = logfilepositionsource
QT = positioning core widgets


HEADERS = logfilepositionsource.h \
          clientapplication.h
SOURCES = logfilepositionsource.cpp \
          clientapplication.cpp \
          main.cpp

RESOURCES += \
    logfile.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/positioning/logfilepositionsource
INSTALLS += target
