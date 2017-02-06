TEMPLATE = subdirs

SUBDIRS += \
           binding \
           compilation \
           javascript \
           holistic \
           qqmlchangeset \
           qqmlcomponent \
           qqmlmetaproperty \
           librarymetrics_performance \
#            script \ ### FIXME: doesn't build
           js \
           creation

qtHaveModule(opengl): SUBDIRS += painting qquickwindow

include(../trusted-benchmarks.pri)
