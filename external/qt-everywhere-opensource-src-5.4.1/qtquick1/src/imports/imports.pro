TEMPLATE = subdirs

SUBDIRS += folderlistmodel particles gestures builtins.pro
qtHaveModule(opengl):!contains(QT_CONFIG, opengles1): SUBDIRS += shaders

qtHaveModule(webkitwidgets): SUBDIRS += webview
