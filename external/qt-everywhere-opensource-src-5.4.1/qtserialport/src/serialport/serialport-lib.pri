INCLUDEPATH += $$PWD

unix {
    greaterThan(QT_MAJOR_VERSION, 4) {
        contains(QT_CONFIG, libudev) {
            DEFINES += LINK_LIBUDEV
            INCLUDEPATH += $$QMAKE_INCDIR_LIBUDEV
            LIBS_PRIVATE += $$QMAKE_LIBS_LIBUDEV
        }
    } else {
        packagesExist(libudev) {
            CONFIG += link_pkgconfig
            DEFINES += LINK_LIBUDEV
            PKGCONFIG += libudev
        }
    }
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
    PRIVATE_HEADERS += \
        $$PWD/qserialport_win_p.h

    SOURCES += \
        $$PWD/qserialport_win.cpp \
        $$PWD/qserialportinfo_win.cpp

    LIBS_PRIVATE += -lsetupapi -ladvapi32
}

wince* {
    PRIVATE_HEADERS += \
        $$PWD/qserialport_wince_p.h

    SOURCES += \
        $$PWD/qserialport_wince.cpp \
        $$PWD/qserialportinfo_wince.cpp
}

symbian {
    MMP_RULES += EXPORTUNFROZEN
    #MMP_RULES += DEBUGGABLE_UDEBONLY
    TARGET.UID3 = 0xE7E62DFD
    TARGET.CAPABILITY =
    TARGET.EPOCALLOWDLLDATA = 1
    addFiles.sources = QtSerialPort.dll
    addFiles.path = !:/sys/bin
    DEPLOYMENT += addFiles

    # FIXME !!!
    #INCLUDEPATH += c:/Nokia/devices/Nokia_Symbian3_SDK_v1.0/epoc32/include/platform
    INCLUDEPATH += c:/QtSDK/Symbian/SDKs/Symbian3Qt473/epoc32/include/platform

    PRIVATE_HEADERS += \
        $$PWD/qserialport_symbian_p.h

    SOURCES += \
        $$PWD/qserialport_symbian.cpp \
        $$PWD/qserialportinfo_symbian.cpp

    LIBS_PRIVATE += -leuser -lefsrv -lc32
}

unix:!symbian {
    PRIVATE_HEADERS += \
        $$PWD/qserialport_unix_p.h

    SOURCES += \
        $$PWD/qserialport_unix.cpp

    !mac {
        SOURCES += \
            $$PWD/qserialportinfo_unix.cpp
    } else {
        SOURCES += \
            $$PWD/qserialportinfo_mac.cpp

        LIBS_PRIVATE += -framework IOKit -framework CoreFoundation
    }
}

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS
