### A qmake file for the snippets. *** This is not for distribution. ***
#
# Snippets that don't work are commented out and marked with "broken";
# other commented out snippets were probably not designed to be built.

TEMPLATE        = subdirs
SUBDIRS         = brush \
                  buffer \
#                  clipboard \      # broken
                  coordsys \
#                  customstyle \
                  designer \
                  dialogs \
                  dockwidgets \
                  draganddrop \
                  dragging \
                  dropactions \
                  dropevents \
                  droprectangle \
                  events \
#                  file \
                  image \
                  inherited-slot \
                  itemselection \
                  layouts \
                  matrix \
                  moc \
#                  modelview-subclasses \   # broken
                  painterpath \
                  persistentindexes \
#                  picture \                # broken
#                  pointer \
                  polygon \
                  process \
                  qdbusextratypes \
                  qcalendarwidget \
                  qdir-filepaths \
                  qdir-listfiles \
                  qdir-namefilters \
                  qfontdatabase \
                  qlabel \
                  qlineargradient \
                  qlistview-dnd \
                  qlistview-using \
                  qlistwidget-dnd \
                  qlistwidget-using \
                  qmetaobject-invokable \
                  qprocess \
                  qprocess-environment \
#                  qsignalmapper \
                  qsortfilterproxymodel-details \
                  qsplashscreen \
                  qstack \
                  qstackedlayout \
                  qstackedwidget \
                  qstandarditemmodel \
                  qstatustipevent \
                  qstring \
                  qstringlist \
                  qstringlistmodel \
                  qstyleoption \
                  qstyleplugin \
                  qsvgwidget \
                  qtablewidget-dnd \
                  qtablewidget-resizing \
                  qtablewidget-using \
                  qtcast \
                  qtreeview-dnd \
                  qtreewidgetitemiterator-using \
                  qtreewidget-using \
                  quiloader \
                  qxmlschema \
                  qxmlschemavalidator \
                  reading-selections \
                  scribe-overview \
                  separations \
#                  settings \               # not designed to be built
                  shareddirmodel \
                  sharedemployee \
                  sharedtablemodel \
#                  signalsandslots \
                  simplemodel-use \
#                  splitter \
                  sqldatabase \
                  stringlistmodel \
#                  styles \
#                  threads \
                  timers \
                  updating-selections \
#                  whatsthis \
                  widget-mask \
                  xml
