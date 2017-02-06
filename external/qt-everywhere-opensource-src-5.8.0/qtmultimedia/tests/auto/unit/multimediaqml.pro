
TEMPLATE = subdirs
SUBDIRS += \
    qdeclarativemultimediaglobal \
    qdeclarativeaudio \
    qdeclarativecamera

disabled {
    SUBDIRS += \
        qdeclarativevideo
}

