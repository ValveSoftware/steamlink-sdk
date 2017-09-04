TEMPLATE = subdirs

SUBDIRS += \
    common \
    l7 \
    npen \
    pointfloat \
    substroke

l7.depends = common
npen.depends = common
pointfloat.depends = common
substroke.depends = common
