TEMPLATE = subdirs

SUBDIRS += \
    inputpanel \
    styles \
    layoutfilesystem \
    layoutresources \

# QTBUG-60268
boot2qt: SUBDIRS = ""
