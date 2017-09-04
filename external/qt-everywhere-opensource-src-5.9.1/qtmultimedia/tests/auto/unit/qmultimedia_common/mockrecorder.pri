INCLUDEPATH *= $$PWD \
    ../../../src/multimedia \
    ../../../src/multimedia/audio \
    ../../../src/multimedia/video \

HEADERS *= \
    ../qmultimedia_common/mockmediarecorderservice.h \
    ../qmultimedia_common/mockmediarecordercontrol.h \
    ../qmultimedia_common/mockvideoencodercontrol.h \
    ../qmultimedia_common/mockaudioencodercontrol.h \
    ../qmultimedia_common/mockaudioinputselector.h \
    ../qmultimedia_common/mockaudioprobecontrol.h \

# We also need all the container/metadata bits
include(mockcontainer.pri)
