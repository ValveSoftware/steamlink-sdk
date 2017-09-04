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

#ifndef TST_QGEOROUTESEGMENT_H
#define TST_QGEOROUTESEGMENT_H

#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QMetaType>

#include <qgeocoordinate.h>
#include <qgeoroutesegment.h>
#include <qgeomaneuver.h>

QT_USE_NAMESPACE

class tst_QGeoRouteSegment : public QObject
{
    Q_OBJECT

public:
    tst_QGeoRouteSegment();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    //Start unit test for QGeoRouteSegment
    void constructor();
    void copy_constructor();
    void destructor();
    void travelTime();
    void travelTime_data();
    void distance();
    void distance_data();
    void path();
    void path_data();
    void maneuver();
    void nextroutesegment();
    void operators();
    void operators_data();
    //End Unit Test for QGeoRouteSegment

private:

    QGeoCoordinate *qgeocoordinate;
    QGeoRouteSegment *qgeoroutesegment;
    QGeoManeuver *qgeomaneuver;

};

Q_DECLARE_METATYPE( QList<double>);

#endif // TST_GEOROUTESEGMENT_H

