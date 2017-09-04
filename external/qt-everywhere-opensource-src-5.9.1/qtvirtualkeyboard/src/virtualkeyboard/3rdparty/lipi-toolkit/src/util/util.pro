TEMPLATE = subdirs

SUBDIRS += \
    lib \
    logger

logger.depends = lib
