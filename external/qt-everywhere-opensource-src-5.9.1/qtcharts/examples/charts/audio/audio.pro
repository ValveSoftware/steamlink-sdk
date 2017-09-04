QT += charts multimedia

HEADERS += \
    widget.h \
    xyseriesiodevice.h

SOURCES += \
    main.cpp\
    widget.cpp \
    xyseriesiodevice.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/charts/audio
INSTALLS += target
