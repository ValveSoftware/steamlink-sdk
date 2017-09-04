TEMPLATE = subdirs

SUBDIRS += \
    shaperec \
    wordrec

wordrec.depends = shaperec
