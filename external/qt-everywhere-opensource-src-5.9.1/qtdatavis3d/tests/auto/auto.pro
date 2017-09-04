TEMPLATE = subdirs

!android: SUBDIRS += cpptest

qtHaveModule(quick): SUBDIRS += qmltest

installed_cmake.depends = cmake

# QTBUG-60268
boot2qt: SUBDIRS -= qmltest
