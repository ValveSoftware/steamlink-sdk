TEMPLATE = subdirs

SUBDIRS = macfunctions
macos: SUBDIRS += embeddedqwindow \
          macpasteboardmime \
          mactoolbar
