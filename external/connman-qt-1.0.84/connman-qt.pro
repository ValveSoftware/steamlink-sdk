TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += libconnman-qt

check.depends = all
check.CONFIG = phony recursive
QMAKE_EXTRA_TARGETS += check

# Adds 'coverage' target
include(coverage.pri)
# CONFIG flag to disable qml plugin
!noplugin {
    SUBDIRS += plugin
}
example {
   SUBDIRS += examples/counters
}
equals(QT_MAJOR_VERSION, 4):  {
# CONFIG flag to disable automatic test
    !notests {
    # fixme for qt5
        SUBDIRS += tests
    }
    OTHER_FILES += rpm/connman-qt.spec
}
equals(QT_MAJOR_VERSION, 5):  {
    OTHER_FILES += rpm/connman-qt5.spec
}
