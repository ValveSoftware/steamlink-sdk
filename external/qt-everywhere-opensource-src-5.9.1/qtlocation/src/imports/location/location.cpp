/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtLocation/private/qdeclarativegeoserviceprovider_p.h>
#include <QtLocation/private/qdeclarativegeomap_p.h>

#include <QtLocation/private/qdeclarativegeoroute_p.h>
#include <QtLocation/private/qdeclarativegeoroutemodel_p.h>
#include <QtLocation/private/qdeclarativegeocodemodel_p.h>
#include <QtLocation/private/qdeclarativegeomaneuver_p.h>
#include <QtLocation/private/qdeclarativegeomapitembase_p.h>
#include <QtLocation/private/qdeclarativegeomapquickitem_p.h>
#include <QtLocation/private/qdeclarativegeomapitemview_p.h>
#include <QtLocation/private/qdeclarativegeomaptype_p.h>
#include <QtLocation/private/qdeclarativerectanglemapitem_p.h>
#include <QtLocation/private/qdeclarativecirclemapitem_p.h>
#include <QtLocation/private/qdeclarativeroutemapitem_p.h>
#include <QtLocation/private/qdeclarativepolylinemapitem_p.h>
#include <QtLocation/private/qdeclarativepolygonmapitem_p.h>
#include <QtLocation/private/qdeclarativegeomapparameter_p.h>
#include <QtLocation/private/qdeclarativegeomapcopyrightsnotice_p.h>
#include <QtLocation/private/qdeclarativegeomapitemgroup_p.h>

//Place includes
#include <QtLocation/private/qdeclarativecategory_p.h>
#include <QtLocation/private/qdeclarativeplace_p.h>
#include <QtLocation/private/qdeclarativeplaceattribute_p.h>
#include <QtLocation/private/qdeclarativeplaceicon_p.h>
#include <QtLocation/private/qdeclarativeratings_p.h>
#include <QtLocation/private/qdeclarativesupplier_p.h>
#include <QtLocation/private/qdeclarativeplaceuser_p.h>
#include <QtLocation/private/qdeclarativecontactdetail_p.h>

#include <QtLocation/private/qdeclarativesupportedcategoriesmodel_p.h>
#include <QtLocation/private/qdeclarativesearchresultmodel_p.h>
#include <QtLocation/private/qdeclarativesearchsuggestionmodel_p.h>

#include <QtQml/qqmlextensionplugin.h>

#include <QtCore/QDebug>

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtLocation);
#endif
}

QT_BEGIN_NAMESPACE


class QtLocationDeclarativeModule: public QQmlExtensionPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid
                      FILE "plugin.json")

public:
    QtLocationDeclarativeModule(QObject *parent = 0) : QQmlExtensionPlugin(parent) { initResources(); }
    virtual void registerTypes(const char *uri)
    {
        if (QLatin1String(uri) == QLatin1String("QtLocation")) {

            // @uri QtLocation
            int major = 5;
            int minor = 0;

            // Register the 5.0 types
            // 5.0 is siltent and not advertised

            qmlRegisterType<QDeclarativeGeoServiceProvider          >(uri, major, minor, "Plugin");
            qmlRegisterType<QDeclarativeGeoServiceProviderParameter >(uri, major, minor, "PluginParameter");
            qmlRegisterUncreatableType<QDeclarativeGeoServiceProviderRequirements>(uri, major, minor, "PluginRequirements",
                                        QStringLiteral("PluginRequirements is not intended instantiable by developer."));
            qmlRegisterType<QDeclarativeGeoMap                      >(uri, major, minor, "Map");

            qmlRegisterUncreatableType<QDeclarativeGeoMapItemBase   >(uri, major, minor, "GeoMapItemBase",
                                        QStringLiteral("GeoMapItemBase is not intended instantiable by developer."));
            qmlRegisterType<QDeclarativeGeoMapQuickItem             >(uri, major, minor, "MapQuickItem");
            qmlRegisterType<QDeclarativeGeoMapItemView              >(uri, major, minor, "MapItemView");

            qmlRegisterType<QDeclarativeGeocodeModel                >(uri, major, minor, "GeocodeModel"); // geocoding and reverse geocoding
            qmlRegisterType<QDeclarativeGeoRouteModel               >(uri, major, minor, "RouteModel");
            qmlRegisterType<QDeclarativeGeoRouteQuery               >(uri, major, minor, "RouteQuery");
            qmlRegisterType<QDeclarativeGeoRoute                    >(uri, major, minor, "Route"); // data type
            qmlRegisterType<QDeclarativeGeoRouteSegment             >(uri, major, minor, "RouteSegment");
            qmlRegisterType<QDeclarativeGeoManeuver                 >(uri, major, minor, "RouteManeuver");
            qmlRegisterUncreatableType<QGeoMapPinchEvent >(uri, major, minor, "MapPinchEvent",
                                        QStringLiteral("(Map)PinchEvent is not intended instantiable by developer."));
            qmlRegisterUncreatableType<QQuickGeoMapGestureArea>(uri, major, minor, "MapGestureArea",
                                        QStringLiteral("(Map)GestureArea is not intended instantiable by developer."));
            qmlRegisterUncreatableType<QDeclarativeGeoMapType       >(uri, major, minor, "MapType",
                                        QStringLiteral("MapType is not intended instantiable by developer."));
            qmlRegisterType<QDeclarativeCategory                    >(uri, major, minor, "Category");
            qmlRegisterType<QDeclarativePlaceEditorialModel         >(uri, major, minor, "EditorialModel");
            qmlRegisterType<QDeclarativePlaceImageModel             >(uri, major, minor, "ImageModel");
            qmlRegisterType<QDeclarativePlace                       >(uri, major, minor, "Place");
            qmlRegisterType<QDeclarativePlaceIcon                   >(uri, major, minor, "Icon");
            qmlRegisterType<QDeclarativeRatings                     >(uri, major, minor, "Ratings");
            qmlRegisterType<QDeclarativeReviewModel                 >(uri, major, minor, "ReviewModel");
            qmlRegisterType<QDeclarativeSupplier                    >(uri, major, minor, "Supplier");
            qmlRegisterType<QDeclarativePlaceUser                   >(uri, major, minor, "User");
            qmlRegisterType<QDeclarativeRectangleMapItem            >(uri, major, minor, "MapRectangle");
            qmlRegisterType<QDeclarativeCircleMapItem               >(uri, major, minor, "MapCircle");
            qmlRegisterType<QDeclarativeMapLineProperties>();
            qmlRegisterType<QDeclarativePolylineMapItem             >(uri, major, minor, "MapPolyline");
            qmlRegisterType<QDeclarativePolygonMapItem              >(uri, major, minor, "MapPolygon");
            qmlRegisterType<QDeclarativeRouteMapItem                >(uri, major, minor, "MapRoute");

            qmlRegisterType<QDeclarativeSupportedCategoriesModel    >(uri, major, minor, "CategoryModel");
            qmlRegisterType<QDeclarativeSearchResultModel           >(uri, major, minor, "PlaceSearchModel");
            qmlRegisterType<QDeclarativeSearchSuggestionModel       >(uri, major, minor, "PlaceSearchSuggestionModel");
            qmlRegisterType<QDeclarativePlaceAttribute              >(uri, major, minor, "PlaceAttribute");
            qmlRegisterUncreatableType<QQmlPropertyMap              >(uri, major, minor, "ExtendedAttributes", "ExtendedAttributes instances cannot be instantiated.  "
                                                                   "Only Place types have ExtendedAttributes and they cannot be re-assigned "
                                                                   "(but can be modified).");
            qmlRegisterType<QDeclarativeContactDetail               >(uri, major, minor, "ContactDetail");
            qmlRegisterUncreatableType<QDeclarativeContactDetails   >(uri, major, minor, "ContactDetails", "ContactDetails instances cannot be instantiated.  "
                                                                                                "Only Place types have ContactDetails and they cannot "
                                                                                                "be re-assigned (but can be modified).");

            // Introduction of 5.3 version; existing 5.0 exports automatically become available under 5.3 as well
            // 5.3 is committed QML API despite missing release of QtLocation 5.3

            minor = 5;
            //TODO: this is broken QTBUG-50990
            qmlRegisterUncreatableType<QDeclarativeGeoMapType, 1>(uri, major, minor, "MapType",
                                                  QStringLiteral("MapType is not intended instantiable by developer."));
            minor = 6;
            //TODO: this is broken QTBUG-50990
            qmlRegisterUncreatableType<QQuickGeoMapGestureArea, 1>(uri, major, minor, "MapGestureArea",
                                        QStringLiteral("(Map)GestureArea is not intended instantiable by developer."));

            // Register the 5.8 types
            minor = 8;
            qmlRegisterType<QDeclarativeGeoManeuver>(uri, major, minor, "RouteManeuver");

            // Register the 5.9 types
            minor = 9;
            qmlRegisterType<QDeclarativeGeoMapParameter>(uri, major, minor, "MapParameter");
            qmlRegisterType<QDeclarativeGeoMapCopyrightNotice>(uri, major, minor, "MapCopyrightNotice");
            qmlRegisterType<QDeclarativeGeoMapItemGroup>(uri, major, minor, "MapItemGroup");

            //registrations below are version independent
            qRegisterMetaType<QPlaceCategory>();
            qRegisterMetaType<QPlace>();
            qRegisterMetaType<QPlaceIcon>();
            qRegisterMetaType<QPlaceRatings>();
            qRegisterMetaType<QPlaceSupplier>();
            qRegisterMetaType<QPlaceUser>();
            qRegisterMetaType<QPlaceAttribute>();
            qRegisterMetaType<QPlaceContactDetail>();
        } else {
            qDebug() << "Unsupported URI given to load location QML plugin: " << QLatin1String(uri);
        }
    }
};

#include "location.moc"

QT_END_NAMESPACE
