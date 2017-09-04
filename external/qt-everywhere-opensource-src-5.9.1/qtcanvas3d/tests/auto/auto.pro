TEMPLATE = subdirs
!uikit: SUBDIRS += qmltest

installed_cmake.depends = cmake

# Currently qmltest crashes for boot2qt
boot2qt: SUBDIRS -= qmltest
