TEMPLATE      = subdirs
SUBDIRS      += comapp \
                hierarchy \
                menus \
                multiple \
                simple \
                wrapper

contains(QT_CONFIG, shared):SUBDIRS += webbrowser
contains(QT_CONFIG, opengl):!contains(QT_CONFIG, opengles2):!contains(QT_CONFIG, dynamicgl): SUBDIRS += opengl

# For now only the contain examples with mingw, for the others you need
# an IDL compiler
mingw|wince*:SUBDIRS = webbrowser
