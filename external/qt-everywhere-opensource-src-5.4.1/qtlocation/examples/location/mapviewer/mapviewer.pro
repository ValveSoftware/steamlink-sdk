TARGET = qml_location_mapviewer
TEMPLATE = app

QT += qml network quick
SOURCES += qmlmapviewerwrapper.cpp

RESOURCES += \
    mapviewerwrapper.qrc

qmlcontent.files += \
    mapviewer.qml \
    demo.ogv
OTHER_FILES += $$qmlcontent.files

qmlcontentmap.files += \
    content/map/MapComponent.qml \
    content/map/Marker.qml \
    content/map/CircleItem.qml \
    content/map/RectangleItem.qml \
    content/map/PolylineItem.qml \
    content/map/PolygonItem.qml \
    content/map/ImageItem.qml \
    content/map/VideoItem.qml \
    content/map/3dItem.qml \
    content/map/MiniMap.qml
OTHER_FILES += $$qmlcontentmap.files

qmlcontentdialogs.files += \
    content/dialogs/Message.qml \
    content/dialogs/RouteDialog.qml
OTHER_FILES += $$qmlcontentdialogs.files

include(../common/common.pri)

target.path = $$[QT_INSTALL_EXAMPLES]/location/mapviewer
additional.files = ../common
additional.path = $$[QT_INSTALL_EXAMPLES]/location/common
INSTALLS += target additional

# ensure copying of media file while shadow building
!equals($${_PRO_FILE_PWD_}, $${OUT_PWD}) {
    MEDIAFILE = $${_PRO_FILE_PWD_}/demo.ogv
    copy2build.input = MEDIAFILE
    copy2build.output = $${OUT_PWD}/demo.ogv
    !contains(TEMPLATE_PREFIX, vc):copy2build.variable_out = PRE_TARGETDEPS
    copy2build.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
    copy2build.name = COPY ${QMAKE_FILE_IN}
    copy2build.CONFIG += no_link
    copy2build.CONFIG += no_clean
    QMAKE_EXTRA_COMPILERS += copy2build
}
