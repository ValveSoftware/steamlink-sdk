/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef TST_QGEOROUTEREQUEST_H
#define TST_QGEOROUTEREQUEST_H

#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QMetaType>

#include <qgeocoordinate.h>
#include <QtPositioning/QGeoRectangle>
#include <qgeorouterequest.h>


QT_USE_NAMESPACE

class tst_QGeoRouteRequest : public QObject
{
    Q_OBJECT

public:
    tst_QGeoRouteRequest();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    //Start Unit Test for QGeoRouteRequest
    void constructor_waypoints();
    void constructor_orig_dest();
    void copy_constructor();
    void destructor();
    void excludeAreas();
    void numberAlternativeRoutes();
    void routeOptimization();
    void routeOptimization_data();
    void segmentDetail();
    void segmentDetail_data();
    void travelModes();
    void travelModes_data();
    void waypoints();
    void waypoints_data();
    void maneuverDetail();
    void maneuverDetail_data();
    void featureWeight();
    void featureWeight_data();
    //End Unit Test for QGeoRouteRequest

private:
    QGeoCoordinate *qgeocoordinate;
    QGeoRectangle *qgeoboundingbox;
    QGeoRouteRequest *qgeorouterequest;

};

Q_DECLARE_METATYPE( QList<double>);
Q_DECLARE_METATYPE( QGeoRouteRequest::RouteOptimization );
Q_DECLARE_METATYPE( QGeoRouteRequest::SegmentDetail );
Q_DECLARE_METATYPE( QGeoRouteRequest::TravelMode );
Q_DECLARE_METATYPE( QGeoRouteRequest::FeatureType);
Q_DECLARE_METATYPE( QGeoRouteRequest::FeatureWeight);
Q_DECLARE_METATYPE( QGeoRouteRequest::ManeuverDetail );

#endif // TST_QGEOROUTEREQUEST_H

