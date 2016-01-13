/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "locationvaluetypeprovider.h"

#include <QtPositioning/private/qdeclarativegeoaddress_p.h>
#include <QtPositioning/private/qdeclarativegeolocation_p.h>

#include "qdeclarativepositionsource_p.h"
#include "qdeclarativeposition_p.h"

#include "qdeclarativegeoshape.h"
#include "qdeclarativegeorectangle.h"
#include "qdeclarativegeocircle.h"
#include "qdeclarativecoordinate_p.h"
#include "qdeclarativegeocoordinateanimation_p.h"

#include "locationsingleton.h"

#include <QtCore/QVariantAnimation>

#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>
#include <QtQml/private/qqmlvaluetype_p.h>
#include <QtQml/private/qqmlglobal_p.h>
#include <QtQml/private/qqmlmetatype_p.h>

#include <QtPositioning/QGeoRectangle>
#include <QtPositioning/QGeoCircle>
#include <QtPositioning/QGeoLocation>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

static QObject *singleton_type_factory(QQmlEngine *engine, QJSEngine *jsEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(jsEngine)

    return new LocationSingleton;
}

static LocationValueTypeProvider *getValueTypeProvider()
{
    static LocationValueTypeProvider provider;
    return &provider;
}

class QtPositioningDeclarativeModule: public QQmlExtensionPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface/1.0"
                      FILE "plugin.json")

public:
    virtual void registerTypes(const char *uri)
    {
        if (QLatin1String(uri) == QStringLiteral("QtPositioning")) {

            // @uri QtPositioning 5.0

            int major = 5;
            int minor = 0;

            qRegisterMetaType<QGeoCoordinate>("QGeoCoordinate");
            qRegisterMetaType<QGeoAddress>("QGeoAddress");
            qRegisterMetaType<QGeoRectangle>("QGeoRectangle");
            qRegisterMetaType<QGeoCircle>("QGeoCircle");
            qRegisterMetaType<QGeoLocation>("QGeoLocation");

            qRegisterAnimationInterpolator<QGeoCoordinate>(geoCoordinateInterpolator);
            QQml_addValueTypeProvider(getValueTypeProvider());

            // Register the 5.0 types
            // 5.0 is silent and not advertised
            qmlRegisterSingletonType<LocationSingleton  >(uri, major, minor, "QtPositioning", singleton_type_factory);
            qmlRegisterValueTypeEnums<GeoShapeValueType >(uri, major, minor, "GeoShape");
            qmlRegisterType<QDeclarativePosition        >(uri, major, minor, "Position");
            qmlRegisterType<QDeclarativePositionSource  >(uri, major, minor, "PositionSource");
            qmlRegisterType<QDeclarativeGeoAddress      >(uri, major, minor, "Address");
            qmlRegisterType<QDeclarativeGeoLocation     >(uri, major, minor, "Location");

            // Register the 5.3 types
            // Introduction of 5.3 version; existing 5.0 exports become automatically available under 5.3
            minor = 3;
            qmlRegisterType<QDeclarativeGeoCoordinateAnimation  >(uri, major, minor, "CoordinateAnimation");
            qmlRegisterType<QDeclarativePosition, 1             >(uri, major, minor, "Position");

            // Register the 5.4 types
            // Introduction of 5.4 version; existing 5.3 exports become automatically available under 5.4
            minor = 4;
            qmlRegisterType<QDeclarativePosition, 2>(uri, major, minor, "Position");
        } else {
            qDebug() << "Unsupported URI given to load positioning QML plugin: " << QLatin1String(uri);
        }
    }
};

#include "positioning.moc"

QT_END_NAMESPACE
