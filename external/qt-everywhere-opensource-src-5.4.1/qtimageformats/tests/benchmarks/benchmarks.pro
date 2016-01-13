TEMPLATE = subdirs
SUBDIRS =
contains(QT_CONFIG, system-zlib): SUBDIRS += mng tiff
