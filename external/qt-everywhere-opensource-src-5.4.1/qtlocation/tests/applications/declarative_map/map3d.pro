TARGET = qml_location_map3d
TEMPLATE=app

QT += network quick

SOURCES += qmlmap3d.cpp

RESOURCES += \
    map3d.qrc

target.path = $$[QT_INSTALL_DEMOS]/qtlocation/declarative/mapviewer
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS *.pro *.qml *.png *.sci
sources.path = $$[QT_INSTALL_DEMOS]/qtlocation/declarative/mapviewer

INSTALLS += target sources

OTHER_FILES += \
    map3d.qml
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
