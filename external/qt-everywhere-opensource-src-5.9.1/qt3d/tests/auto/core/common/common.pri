SOURCES += \
    $$PWD/testpostmanarbiter.cpp

HEADERS += \
    $$PWD/testpostmanarbiter.h

INCLUDEPATH += $$PWD

qtConfig(private_tests) {
    SOURCES += \
        $$PWD/qbackendnodetester.cpp

    HEADERS += \
        $$PWD/qbackendnodetester.h
}

QT += core-private 3dcore 3dcore-private
