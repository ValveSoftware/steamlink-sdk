TEMPLATE = subdirs

unix {
    CONFIG += link_pkgconfig
    packagesExist(speech-dispatcher) {
        SUBDIRS += speechdispatcher
    }
}

windows:!winrt: SUBDIRS += sapi
winrt: SUBDIRS += winrt

osx: SUBDIRS += osx
uikit: SUBDIRS += ios

android: SUBDIRS += android

config_flite | config_flite_alsa {
    SUBDIRS += flite
}

config_vocalizer: exists(vocalizer) {
    SUBDIRS += vocalizer
}
