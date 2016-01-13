TEMPLATE = subdirs
contains(QT_CONFIG, xcb) {
  SUBDIRS += x11extras
}
