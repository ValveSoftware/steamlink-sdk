QT = core serialbus

SOURCES += main.cpp \
    readtask.cpp \
    canbusutil.cpp \
    sigtermhandler.cpp

HEADERS += \
    readtask.h \
    canbusutil.h \
    sigtermhandler.h

load(qt_tool)
