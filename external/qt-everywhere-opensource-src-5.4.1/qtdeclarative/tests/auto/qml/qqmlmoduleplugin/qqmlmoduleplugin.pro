QT = core
TEMPLATE = subdirs
SUBDIRS =\
    plugin\
    plugin.2\
    plugin.2.1\
    pluginWrongCase\
    pluginWithQmlFile\
    pluginMixed\
    pluginVersion\
    nestedPlugin\
    strictModule\
    invalidStrictModule\
    nonstrictModule\
    preemptiveModule\
    preemptedStrictModule\
    invalidNamespaceModule\
    invalidFirstCommandModule\
    protectedModule

tst_qqmlmoduleplugin_pro.depends += plugin
SUBDIRS += tst_qqmlmoduleplugin.pro

CONFIG += parallel_test

QT += core-private gui-private qml-private
