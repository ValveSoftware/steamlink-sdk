TARGET = QtPositioning
QT = core-private

QMAKE_DOCS = $$PWD/doc/qtpositioning.qdocconf
OTHER_FILES += doc/src/*.qdoc   # show .qdoc files in Qt Creator

ANDROID_BUNDLED_JAR_DEPENDENCIES = \
    jar/QtPositioning-bundled.jar:org.qtproject.qt5.android.positioning.QtPositioning
ANDROID_JAR_DEPENDENCIES = \
    jar/QtPositioning.jar:org.qtproject.qt5.android.positioning.QtPositioning
ANDROID_PERMISSIONS = \
    android.permission.ACCESS_FINE_LOCATION
ANDROID_LIB_DEPENDENCIES = \
    plugins/position/libqtposition_android.so
MODULE_WINRT_CAPABILITIES_DEVICE += \
    location
MODULE_PLUGIN_TYPES = \
    position

PUBLIC_HEADERS += \
                    qgeoaddress.h \
                    qgeoareamonitorinfo.h \
                    qgeoareamonitorsource.h \
                    qgeoshape.h \
                    qgeorectangle.h \
                    qgeocircle.h \
                    qgeocoordinate.h \
                    qgeolocation.h \
                    qgeopositioninfo.h \
                    qgeopositioninfosource.h \
                    qgeosatelliteinfo.h \
                    qgeosatelliteinfosource.h \
                    qnmeapositioninfosource.h \
                    qgeopositioninfosourcefactory.h \
                    qpositioningglobal.h

PRIVATE_HEADERS += \
                    qgeoaddress_p.h \
                    qgeoshape_p.h \
                    qgeorectangle_p.h \
                    qgeocircle_p.h \
                    qgeolocation_p.h \
                    qlocationutils_p.h \
                    qnmeapositioninfosource_p.h \
                    qgeocoordinate_p.h \
                    qgeopositioninfosource_p.h \
                    qdeclarativegeoaddress_p.h \
                    qdeclarativegeolocation_p.h \
                    qdoublevector2d_p.h \
                    qdoublevector3d_p.h \
                    qgeoprojection_p.h \
                    qpositioningglobal_p.h \
                    qlocationdata_simulator_p.h

SOURCES += \
            qgeoaddress.cpp \
            qgeoareamonitorsource.cpp \
            qgeoareamonitorinfo.cpp \
            qgeoshape.cpp \
            qgeorectangle.cpp \
            qgeocircle.cpp \
            qgeocoordinate.cpp \
            qgeolocation.cpp \
            qgeopositioninfo.cpp \
            qgeopositioninfosource.cpp \
            qgeosatelliteinfo.cpp \
            qgeosatelliteinfosource.cpp \
            qlocationutils.cpp \
            qnmeapositioninfosource.cpp \
            qgeopositioninfosourcefactory.cpp \
            qdeclarativegeoaddress.cpp \
            qdeclarativegeolocation.cpp \
            qdoublevector2d.cpp \
            qdoublevector3d.cpp \
            qgeoprojection.cpp \
            qlocationdata_simulator.cpp

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS


load(qt_module)
