TEMPLATE = subdirs
SUBDIRS = activeqt
win32:!winrt: SUBDIRS += tools
CONFIG += ordered
