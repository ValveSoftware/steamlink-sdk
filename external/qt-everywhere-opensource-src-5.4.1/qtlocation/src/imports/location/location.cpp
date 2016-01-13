/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdeclarativegeoserviceprovider_p.h"
#include "qdeclarativegeomap_p.h"

#include "qdeclarativegeoroute_p.h"
#include "qdeclarativegeoroutemodel_p.h"
#include "qdeclarativegeocodemodel_p.h"
#include "qdeclarativegeomaneuver_p.h"
#include "qdeclarativegeomapquickitem_p.h"
#include "qdeclarativegeomapitemview_p.h"
#include "qdeclarativegeomaptype_p.h"
#include "qdeclarativerectanglemapitem_p.h"
#include "qdeclarativecirclemapitem_p.h"
#include "qdeclarativeroutemapitem_p.h"
#include "qdeclarativepolylinemapitem_p.h"
#include "qdeclarativepolygonmapitem_p.h"

//Place includes
#include "qdeclarativecategory_p.h"
#include "qdeclarativeplace_p.h"
#include "qdeclarativeplaceattribute_p.h"
#include "qdeclarativeplaceicon_p.h"
#include "qdeclarativeratings_p.h"
#include "qdeclarativesupplier_p.h"
#include "qdeclarativeplaceuser_p.h"
#include "qdeclarativecontactdetail_p.h"

#include "qdeclarativesupportedcategoriesmodel_p.h"
#include "qdeclarativesearchresultmodel_p.h"
#include "qdeclarativesearchsuggestionmodel_p.h"
#include "error_messages.h"

#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>
#include <QtQml/private/qqmlvaluetype_p.h>
#include <QtQml/private/qqmlglobal_p.h>
#include <QtQml/private/qqmlmetatype_p.h>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE


class QtLocationDeclarativeModule: public QQmlExtensionPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface/1.0"
                      FILE "plugin.json")

public:
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
                                        QStringLiteral("HeoMapItemBase is not intended instantiable by developer."));
            qmlRegisterType<QDeclarativeGeoMapQuickItem             >(uri, major, minor, "MapQuickItem");
            qmlRegisterType<QDeclarativeGeoMapItemView              >(uri, major, minor, "MapItemView");

            qmlRegisterType<QDeclarativeGeocodeModel                >(uri, major, minor, "GeocodeModel"); // geocoding and reverse geocoding
            qmlRegisterType<QDeclarativeGeoRouteModel               >(uri, major, minor, "RouteModel");
            qmlRegisterType<QDeclarativeGeoRouteQuery               >(uri, major, minor, "RouteQuery");
            qmlRegisterType<QDeclarativeGeoRoute                    >(uri, major, minor, "Route"); // data type
            qmlRegisterType<QDeclarativeGeoRouteSegment             >(uri, major, minor, "RouteSegment");
            qmlRegisterType<QDeclarativeGeoManeuver                 >(uri, major, minor, "RouteManeuver");
            qmlRegisterUncreatableType<QDeclarativeGeoMapPinchEvent >(uri, major, minor, "MapPinchEvent",
                                        QStringLiteral("(Map)PinchEvent is not intended instantiable by developer."));
            qmlRegisterUncreatableType<QDeclarativeGeoMapGestureArea>(uri, major, minor, "MapGestureArea",
                                        QStringLiteral("(Map)HestureArea is not intended instantiable by developer."));
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

            // Register the 5.4 types
            // Implicitly registers 5.3
            minor = 4;
            // For now there are no new types; just reregister one existing 5.0 type
            qmlRegisterUncreatableType<QDeclarativeGeoMapType, 1>(uri, major, minor, "MapType",
                                        QStringLiteral("MapType is not intended instantiable by developer."));

            //registrations below are version independent
            qRegisterMetaType<QPlaceCategory>("QPlaceCategory");
            qRegisterMetaType<QPlace>("QPlace");
            qRegisterMetaType<QPlaceIcon>("QPlaceIcon");
            qRegisterMetaType<QPlaceRatings>("QPlaceRatings");
            qRegisterMetaType<QPlaceSupplier>("QPlaceSupplier");
            qRegisterMetaType<QPlaceUser>("QPlaceUser");
            qRegisterMetaType<QPlaceAttribute>("QPlaceAttribute");
            qRegisterMetaType<QPlaceContactDetail>("QPlaceContactDetail");
        } else {
            qDebug() << "Unsupported URI given to load location QML plugin: " << QLatin1String(uri);
        }
    }
};

#include "location.moc"

QT_END_NAMESPACE
