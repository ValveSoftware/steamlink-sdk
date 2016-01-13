QT = core testlib
TEMPLATE = subdirs
SUBDIRS = plugin plugin.2 plugin.2.1 pluginWrongCase pluginWithQmlFile pluginMixed pluginVersion pureQml
tst_qdeclarativemoduleplugin_pro.depends += plugin
SUBDIRS += tst_qdeclarativemoduleplugin.pro

CONFIG += parallel_test

