TEMPLATE=subdirs
SUBDIRS=\
           qscriptable \
           qscriptclass \
           qscriptcontext \
           qscriptcontextinfo \
           qscriptengine \
           qscriptenginedebugger \
           qscriptextensionplugin \
           qscriptextqobject \
           qscriptjstestsuite \
           qscriptstring \
           qscriptv8testsuite \
           qscriptvalue \
           qscriptvaluegenerated \
           qscriptvalueiterator \
           qscriptqwidgets \
           cmake \

!contains(QT_CONFIG, private_tests): SUBDIRS -= \
           qscriptcontext \
           qscriptengineagent

!qtHaveModule(widgets):SUBDIRS -= \
    qscriptable \
    qscriptengine \
    qscriptenginedebugger \
    qscriptextqobject \
    qscriptqwidgets \
    qscriptvalue

!cross_compile:                             SUBDIRS += host.pro
