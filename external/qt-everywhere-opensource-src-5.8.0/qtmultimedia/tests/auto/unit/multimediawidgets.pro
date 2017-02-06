
TEMPLATE = subdirs
SUBDIRS += \
    qcameraviewfinder \
    qcamerawidgets \
    qmediaplayerwidgets \

# Tests depending on private interfaces should only be built if
# these interfaces are exported.
qtConfig(private_tests) {
  SUBDIRS += \
    qgraphicsvideoitem \
    qpaintervideosurface \
    qvideowidget
}

