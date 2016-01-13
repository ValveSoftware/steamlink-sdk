INCLUDEPATH += $$PWD

LIBS += -lstrmiids -ldmoguids -luuid -lmsdmo -lole32 -loleaut32 -lMf -lMfuuid -lMfplat \
        -lPropsys -lmfreadwrite -lwmcodecdspuuid

HEADERS += \
    $$PWD/mfdecoderservice.h \
    $$PWD/mfdecodersourcereader.h \
    $$PWD/mfaudiodecodercontrol.h

SOURCES += \
    $$PWD/mfdecoderservice.cpp \
    $$PWD/mfdecodersourcereader.cpp \
    $$PWD/mfaudiodecodercontrol.cpp