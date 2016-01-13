TEMPLATE = subdirs
wince*: contains(QT_CONFIG, cetest): SUBDIRS += wince
CONFIG += ordered
