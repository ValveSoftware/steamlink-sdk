/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef TST_QGEOROUTE_H
#define TST_QGEOROUTE_H

#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QMetaType>

#include <qgeoroute.h>
#include <QtPositioning/QGeoRectangle>
#include <qgeocoordinate.h>
#include <qgeorouterequest.h>
#include <qgeoroutesegment.h>


QT_USE_NAMESPACE

class tst_QGeoRoute : public QObject
{
    Q_OBJECT

public:
    tst_QGeoRoute();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    //Start unit test for QGeoRoute
    void constructor();
    void copy_constructor();
    void destructor();
    void bounds();
    void distance();
    void path();
    void path_data();
    void request();
    void routeId();
    void firstrouteSegments();
    void travelMode();
    void travelMode_data();
    void travelTime();
    void operators();
    //End Unit Test for QGeoRoute

private:
    QGeoRoute *qgeoroute;
    QGeoRectangle *qgeoboundingbox;
    QGeoCoordinate *qgeocoordinate;
    QGeoRouteRequest *qgeorouterequest;
    QGeoRouteSegment *qgeoroutesegment;


};

Q_DECLARE_METATYPE( QList<double>);
Q_DECLARE_METATYPE( QList<QGeoCoordinate>);
Q_DECLARE_METATYPE( QGeoRouteRequest::TravelMode);


#endif // TST_QGEOROUTE_H
