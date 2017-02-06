TEMPLATE = subdirs

!android: SUBDIRS += cpptest

qtHaveModule(quick): SUBDIRS += qmltest

installed_cmake.depends = cmake
