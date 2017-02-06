TEMPLATE = subdirs

SUBDIRS += \
    cmake \
    oauth1 \
    oauth1signature \
    oauthhttpserverreplyhandler

qtConfig(private_tests) {
    SUBDIRS += abstractoauth
}
