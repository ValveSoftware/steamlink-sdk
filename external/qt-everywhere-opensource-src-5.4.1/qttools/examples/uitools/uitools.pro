TEMPLATE      = subdirs
SUBDIRS       = multipleinheritance

!wince*:contains(QT_BUILD_PARTS, tools): SUBDIRS += textfinder
