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

#include <qgeoroutexmlparser.h>
#include <qgeocoordinate.h>
#include <qtest.h>
#include <qgeoroute.h>

#include <QMetaType>
#include <QDebug>
#include <QFile>
#include <QSignalSpy>

Q_DECLARE_METATYPE(QList<QGeoRoute>)

QT_USE_NAMESPACE

class tst_QGeoRouteXmlParser : public QObject
{
    Q_OBJECT

public:
    tst_QGeoRouteXmlParser()
        : start(0.0, 0.0),
          end(1.0, 1.0)
    {
        qRegisterMetaType<QList<QGeoRoute> >();
    }

private:
    // dummy values for creating the request object
    QGeoCoordinate start;
    QGeoCoordinate end;

private slots:
    void test_realData1()
    {
        QFile f(":/route1.xml");
        if (!f.open(QIODevice::ReadOnly))
            QFAIL("could not open route1.xml");

        QGeoRouteRequest req(start, end);
        QGeoRouteXmlParser xp(req);
        xp.setAutoDelete(false);

        QSignalSpy resultsSpy(&xp, SIGNAL(results(QList<QGeoRoute>)));

        xp.parse(f.readAll());

        QTRY_COMPARE(resultsSpy.count(), 1);

        QVariantList arguments = resultsSpy.first();

        // xml contains exactly 1 route
        QList<QGeoRoute> results = arguments.at(0).value<QList<QGeoRoute> >();
        QCOMPARE(results.size(), 1);
        QGeoRoute route = results.first();

        QList<QGeoRouteSegment> segments;
        // get all the segments on the route
        segments << route.firstRouteSegment();
        while (segments.last().isValid())
            segments << segments.last().nextRouteSegment();

        // should be 9 segments in the list (last one invalid)
        QCOMPARE(segments.size(), 9);

        // check the first maneuver is correct
        QGeoManeuver first = segments.at(0).maneuver();
        QCOMPARE(first.instructionText(), QStringLiteral("Head toward Logan Rd (95) on Padstow Rd (56). Go for 0.3 miles."));
        QCOMPARE(first.position(), QGeoCoordinate(-27.5752144, 153.0879669));

        QCOMPARE(first.timeToNextInstruction(), 24);
        QCOMPARE(first.distanceToNextInstruction(), 403.0);

        // check the last two maneuvers -- route1.xml has a directionless final maneuver
        QGeoManeuver secondLast = segments.at(6).maneuver();
        QVERIFY(secondLast.instructionText().contains("Turn right onto Bartley St"));
        QCOMPARE(secondLast.position(), QGeoCoordinate(-27.4655991, 153.0231628));
        QCOMPARE(secondLast.distanceToNextInstruction(), 181.0);
        QCOMPARE(secondLast.timeToNextInstruction(), 41);

        QGeoManeuver last = segments.at(7).maneuver();
        QVERIFY(last.instructionText().contains("Arrive at Bartley St"));
        QCOMPARE(last.position(), QGeoCoordinate(-27.4650097, 153.0230255));
        QCOMPARE(last.distanceToNextInstruction(), 0.0);
        QCOMPARE(last.timeToNextInstruction(), 0);
    }

    void test_realData2()
    {
        QFile f(":/route2.xml");
        if (!f.open(QIODevice::ReadOnly))
            QFAIL("could not open route2.xml");

        QGeoRouteRequest req(start, end);
        QGeoRouteXmlParser xp(req);
        xp.setAutoDelete(false);

        QSignalSpy resultsSpy(&xp, SIGNAL(results(QList<QGeoRoute>)));

        xp.parse(f.readAll());

        QTRY_COMPARE(resultsSpy.count(), 1);

        QVariantList arguments = resultsSpy.first();

        // xml contains exactly 1 route
        QList<QGeoRoute> results = arguments.at(0).value<QList<QGeoRoute> >();
        QCOMPARE(results.size(), 1);
        QGeoRoute route = results.first();

        QList<QGeoRouteSegment> segments;
        // get all the segments on the route
        segments << route.firstRouteSegment();
        while (segments.last().isValid())
            segments << segments.last().nextRouteSegment();

        // should be 14 segments in the list (last one invalid)
        QCOMPARE(segments.size(), 14);

        QCOMPARE(route.path().size(), 284);
        QCOMPARE(route.path().at(57), QGeoCoordinate(-27.5530605, 153.0691223));
        QCOMPARE(route.path().at(283), QGeoCoordinate(-27.4622307, 153.0397949));

        QVERIFY(segments.at(0).maneuver().instructionText().contains("Head toward Electronics St"));
        QCOMPARE(segments.at(0).maneuver().direction(), QGeoManeuver::DirectionForward);
        QVERIFY(segments.at(1).maneuver().instructionText().contains("Turn left onto Miles Platting"));
        QCOMPARE(segments.at(1).maneuver().direction(), QGeoManeuver::DirectionLeft);
        QVERIFY(segments.at(2).maneuver().instructionText().contains("Turn right onto Logan Rd"));
        QCOMPARE(segments.at(2).maneuver().direction(), QGeoManeuver::DirectionRight);
        QVERIFY(segments.at(3).maneuver().instructionText().contains("Take exit #14/M3/City"));
        QCOMPARE(segments.at(3).maneuver().direction(), QGeoManeuver::DirectionLightLeft);
        QVERIFY(segments.at(4).maneuver().instructionText().contains("Take exit #2/41"));
        QCOMPARE(segments.at(4).maneuver().direction(), QGeoManeuver::DirectionLightLeft);
        QVERIFY(segments.at(5).maneuver().instructionText().contains("Turn right onto Allen St"));
        QCOMPARE(segments.at(5).maneuver().direction(), QGeoManeuver::DirectionRight);
        QVERIFY(segments.at(6).maneuver().instructionText().contains("Bear right to stay on"));
        QCOMPARE(segments.at(6).maneuver().direction(), QGeoManeuver::DirectionLightRight);
        QVERIFY(segments.at(7).maneuver().instructionText().contains("Bear right onto Vulture St"));
        QCOMPARE(segments.at(7).maneuver().direction(), QGeoManeuver::DirectionLightRight);
    }
};

QTEST_GUILESS_MAIN(tst_QGeoRouteXmlParser)
#include "tst_qgeoroutexmlparser.moc"

