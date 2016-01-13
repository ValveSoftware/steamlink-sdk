TEMPLATE = subdirs

contains(QT_CONFIG, xcb) {
  SUBDIRS += auto
}
