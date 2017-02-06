INCLUDEPATH += $$PWD

LIBS += -lmfreadwrite -lwmcodecdspuuid
QMAKE_USE += wmf

HEADERS += \
    $$PWD/mfdecoderservice.h \
    $$PWD/mfdecodersourcereader.h \
    $$PWD/mfaudiodecodercontrol.h

SOURCES += \
    $$PWD/mfdecoderservice.cpp \
    $$PWD/mfdecodersourcereader.cpp \
    $$PWD/mfaudiodecodercontrol.cpp
