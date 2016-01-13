TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += websockets
qtHaveModule(quick): SUBDIRS += imports
