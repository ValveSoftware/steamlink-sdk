TEMPLATE = subdirs

qtHaveModule(widgets) {
    SUBDIRS += widgets
    qtHaveModule(quick) {
      SUBDIRS += quick
  }
}
