TARGET = tst_patternistexamples
CONFIG += testcase
SOURCES += tst_patternistexamples.cpp
QT += testlib
wince*: {
    snippets.files = ../../../doc/src/snippets/patternist/*
    snippets.path = patternist
    widgetRen.files = ../../../examples/xquery/widgetRenderer/*
    widgetRen.path = widgetRenderer
    globVar.files = ../../../examples/xquery/globalVariables/*
    globVar.path = globalVariables
    filetree.files = ../../../examples/filetree/*
    filetree.path = filetree
    recipes.files = ../../../examples/recipes/*
    recipes.path = recipes
    files.files = ../../../examples/recipes/files/*
    files.path = recipes\\files

    DEPLOYMENT += snippets widgetRen globVar filetree recipes files
    # take care of dependency
    QT += network
} else {
    DEFINES += SOURCETREE=\\\"$$absolute_path(../../..)/\\\"
}
include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
