TEMPLATE = subdirs

SUBDIRS += lib plugin console_app

qtHaveModule(quick): SUBDIRS += import qml.pro

plugin.depends = lib
import.depends = lib

