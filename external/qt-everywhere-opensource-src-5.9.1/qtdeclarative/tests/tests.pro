TEMPLATE = subdirs
SUBDIRS +=  auto
contains(QT_CONFIG, release): SUBDIRS += benchmarks
