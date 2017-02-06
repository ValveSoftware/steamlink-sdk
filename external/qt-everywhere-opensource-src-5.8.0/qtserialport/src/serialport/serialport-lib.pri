INCLUDEPATH += $$PWD

unix:qtConfig(libudev) {
    DEFINES += LINK_LIBUDEV
    INCLUDEPATH += $$QMAKE_INCDIR_LIBUDEV
    LIBS_PRIVATE += $$QMAKE_LIBS_LIBUDEV
}

PUBLIC_HEADERS += \
    $$PWD/qserialportglobal.h \
    $$PWD/qserialport.h \
    $$PWD/qserialportinfo.h

PRIVATE_HEADERS += \
    $$PWD/qserialport_p.h \
    $$PWD/qserialportinfo_p.h

SOURCES += \
    $$PWD/qserialport.cpp \
    $$PWD/qserialportinfo.cpp

win32:!wince* {
    SOURCES += \
        $$PWD/qserialport_win.cpp \
        $$PWD/qserialportinfo_win.cpp

    LIBS_PRIVATE += -lsetupapi -ladvapi32
}

unix {
    SOURCES += \
        $$PWD/qserialport_unix.cpp

    osx {
        SOURCES += \
            $$PWD/qserialportinfo_osx.cpp

        LIBS_PRIVATE += -framework IOKit -framework CoreFoundation
    } else:freebsd {
        SOURCES += \
            $$PWD/qserialportinfo_freebsd.cpp
    } else {
        SOURCES += \
            $$PWD/qserialportinfo_unix.cpp
    }
}

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS
