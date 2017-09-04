TEMPLATE = subdirs
SUBDIRS += auto

qtHaveModule(bluetooth):qtHaveModule(quick): SUBDIRS += bttestui
