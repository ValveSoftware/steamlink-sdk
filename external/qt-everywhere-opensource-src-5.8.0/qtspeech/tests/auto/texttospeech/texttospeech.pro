CONFIG += testcase
TARGET = tst_qtexttospeech
QT += testlib core texttospeech
SOURCES += tst_qtexttospeech.cpp

unix {
    CONFIG += link_pkgconfig
    packagesExist(speech-dispatcher) {
        PKGCONFIG = speech-dispatcher
        DEFINES += HAVE_SPEECHD
    }
}
