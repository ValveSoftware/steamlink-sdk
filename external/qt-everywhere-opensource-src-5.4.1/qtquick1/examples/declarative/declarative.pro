TEMPLATE = subdirs

# These examples contain some C++ and need to be built
SUBDIRS = \
    demos \
    animation \
    cppextensions \
    flickr \
    i18n \
    imageelements \
    keyinteraction/focus \
    modelviews \
    positioners \
    righttoleft \
    sqllocalstorage \
    text \
    threading \
    touchinteraction \
    toys \
    tutorials \
    ui-components

# OpenGL shader examples requires opengl and they contain some C++ and need to be built
qtHaveModule(opengl): SUBDIRS += shadereffects

# These examples contain no C++ and can simply be copied
EXAMPLE_FILES = \
    helper \
    screenorientation \
    xml
