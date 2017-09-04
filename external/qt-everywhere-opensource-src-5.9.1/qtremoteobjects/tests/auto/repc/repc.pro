TEMPLATE = subdirs
SUBDIRS += enums pods
!windows: SUBDIRS += signature # QTBUG-58773
