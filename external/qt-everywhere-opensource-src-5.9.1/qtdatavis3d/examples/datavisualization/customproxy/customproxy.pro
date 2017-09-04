android|ios|winrt {
    error( "This example is not supported for android, ios, or winrt." )
}

!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

SOURCES += main.cpp \
    rainfallgraph.cpp \
    variantdataset.cpp \
    variantbardataproxy.cpp \
    variantbardatamapping.cpp \

HEADERS += \
    rainfallgraph.h \
    variantdataset.h \
    variantbardataproxy.h \
    variantbardatamapping.h

RESOURCES += customproxy.qrc

OTHER_FILES += doc/src/* \
               doc/images/* \
               data/raindata.txt
