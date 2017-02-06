TEMPLATE = subdirs
SUBDIRS = \
        qjsengine \
#        qjsvalue \ ### FIXME: doesn't build
        qjsvalueiterator \

TRUSTED_BENCHMARKS += \
    qjsvalue \
    qjsengine \

