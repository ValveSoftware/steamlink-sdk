TEMPLATE = subdirs

SUBDIRS += qmlvideo qmlvideofx

EXAMPLE_FILES += \
    qmlvideofilter_opencl \   # FIXME: this one should use a configure check instead
    snippets
