TEMPLATE = subdirs
SUBDIRS += \
    conversion \
    qaxobject \
    qaxscript \
    dumpcpp \
    cmake

*g++*: SUBDIRS -= \
    qaxscript \
