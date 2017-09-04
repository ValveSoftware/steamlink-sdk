TEMPLATE = subdirs

SUBDIRS += \
    common \
    boxfld

boxfld.depends = common
