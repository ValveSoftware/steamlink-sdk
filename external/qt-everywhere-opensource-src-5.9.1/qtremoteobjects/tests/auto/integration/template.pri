#Only needed for tests since the temperature.h file is
#in the folder with the .pri file, not the folder with
#the .pro file
INCLUDEPATH += $$PWD

OTHER_FILES = $$PWD/engine.rep \
              $$PWD/../repfiles/localdatacenter.rep \
              $$PWD/../repfiles/tcpdatacenter.rep

REPC_SOURCE += $$OTHER_FILES
REPC_REPLICA += $$OTHER_FILES
REPC_MERGED += $$PWD/speedometer.rep $$PWD/enum.rep $$PWD/pod.rep

HEADERS += $$PWD/engine.h \
           $$PWD/speedometer.h \
           $$PWD/temperature.h

SOURCES += $$PWD/engine.cpp \
           $$PWD/speedometer.cpp \
           $$PWD/tst_integration.cpp

contains(QT_CONFIG, c++11): CONFIG += c++11
