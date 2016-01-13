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

#ifndef QGEOROUTINGMANAGERENGINE_H
#define QGEOROUTINGMANAGERENGINE_H

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QLocale>
#include <QtLocation/QGeoRouteRequest>
#include <QtLocation/QGeoRouteReply>

QT_BEGIN_NAMESPACE

class QGeoRoutingManagerEnginePrivate;

class Q_LOCATION_EXPORT QGeoRoutingManagerEngine : public QObject
{
    Q_OBJECT
public:
    QGeoRoutingManagerEngine(const QVariantMap &parameters, QObject *parent = 0);
    virtual ~QGeoRoutingManagerEngine();

    QString managerName() const;
    int managerVersion() const;

    virtual QGeoRouteReply *calculateRoute(const QGeoRouteRequest &request) = 0;
    virtual QGeoRouteReply *updateRoute(const QGeoRoute &route, const QGeoCoordinate &position);

    QGeoRouteRequest::TravelModes supportedTravelModes() const;
    QGeoRouteRequest::FeatureTypes supportedFeatureTypes() const;
    QGeoRouteRequest::FeatureWeights supportedFeatureWeights() const;
    QGeoRouteRequest::RouteOptimizations supportedRouteOptimizations() const;
    QGeoRouteRequest::SegmentDetails supportedSegmentDetails() const;
    QGeoRouteRequest::ManeuverDetails supportedManeuverDetails() const;

    void setLocale(const QLocale &locale);
    QLocale locale() const;
    void setMeasurementSystem(QLocale::MeasurementSystem system);
    QLocale::MeasurementSystem measurementSystem() const;

Q_SIGNALS:
    void finished(QGeoRouteReply *reply);
    void error(QGeoRouteReply *reply, QGeoRouteReply::Error error, QString errorString = QString());

protected:
    void setSupportedTravelModes(QGeoRouteRequest::TravelModes travelModes);
    void setSupportedFeatureTypes(QGeoRouteRequest::FeatureTypes featureTypes);
    void setSupportedFeatureWeights(QGeoRouteRequest::FeatureWeights featureWeights);
    void setSupportedRouteOptimizations(QGeoRouteRequest::RouteOptimizations optimizations);
    void setSupportedSegmentDetails(QGeoRouteRequest::SegmentDetails segmentDetails);
    void setSupportedManeuverDetails(QGeoRouteRequest::ManeuverDetails maneuverDetails);

private:
    void setManagerName(const QString &managerName);
    void setManagerVersion(int managerVersion);

    QGeoRoutingManagerEnginePrivate *d_ptr;
    Q_DISABLE_COPY(QGeoRoutingManagerEngine)

    friend class QGeoServiceProvider;
    friend class QGeoServiceProviderPrivate;
};

QT_END_NAMESPACE

#endif
