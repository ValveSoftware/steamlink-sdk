TEMPLATE = subdirs
SUBDIRS = activeqt
win32:!winrt:!wince: SUBDIRS += tools
CONFIG += ordered
