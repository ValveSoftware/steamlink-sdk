TEMPLATE = subdirs
SUBDIRS = \
        context2d \
        qscriptclass \
        qscriptclass_bytearray \
        qscriptengine \
        qscriptvalue \
        sunspider \
        qscriptqobject \
        qscriptvalueiterator \
        v8

TRUSTED_BENCHMARKS += \
    qscriptclass \
    qscriptvalue \
    qscriptengine \
    qscriptobject \
    context2d

include(../trusted-benchmarks.pri)
