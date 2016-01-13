TEMPLATE=subdirs
SUBDIRS=\
    qml \
    quick \
    headersclean \
    particles \
    qmltest \
    qmldevtools \
    cmake \
    installed_cmake

qtHaveModule(widgets): SUBDIRS += quickwidgets

qmldevtools.CONFIG = host_build

installed_cmake.depends = cmake

testcocoon: SUBDIRS -= headersclean
