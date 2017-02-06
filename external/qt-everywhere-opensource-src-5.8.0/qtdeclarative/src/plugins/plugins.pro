TEMPLATE = subdirs

!contains(QT_CONFIG, no-qml-debug):SUBDIRS += qmltooling
qtHaveModule(quick):SUBDIRS += scenegraph
