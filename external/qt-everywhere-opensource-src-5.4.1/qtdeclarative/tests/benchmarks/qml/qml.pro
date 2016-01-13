TEMPLATE = subdirs

SUBDIRS += \
           binding \
           creation \
           compilation \
           javascript \
           holistic \
           pointers \
           qqmlcomponent \
           qqmlimage \
           qqmlmetaproperty \
#            script \ ### FIXME: doesn't build
           qmltime \
           js \
           qquickwindow

qtHaveModule(opengl): SUBDIRS += painting

include(../trusted-benchmarks.pri)
