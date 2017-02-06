/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>
#include <QtCore/qdebug.h>

#include <qmediatimerange.h>
#include <qmediatimerange.h>

QT_USE_NAMESPACE

class tst_QMediaTimeRange: public QObject
{
    Q_OBJECT

public slots:

private slots:
    void testCtor();
    void testIntervalCtor();
    void testGetters();
    void testAssignment();
    void testIntervalNormalize();
    void testIntervalTranslate();
    void testIntervalContains();
    void testEarliestLatest();
    void testContains();
    void testAddInterval();
    void testAddTimeRange();
    void testRemoveInterval();
    void testRemoveTimeRange();
    void testClear();
    void testComparisons();
    void testArithmetic();
};

void tst_QMediaTimeRange::testIntervalCtor()
{
    //Default Ctor for Time Interval
    /* create an instance for the time interval and verify the default cases */
    QMediaTimeInterval tInter;
    QVERIFY(tInter.isNormal());
    QVERIFY(tInter.start() == 0);
    QVERIFY(tInter.end() == 0);

    // (qint, qint) Ctor time interval
    /* create an instace of QMediaTimeInterval passing start and end times and verify the all possible scenario's*/
    QMediaTimeInterval time(20,50);
    QVERIFY(time.isNormal());
    QVERIFY(time.start() == 20);
    QVERIFY(time.end() == 50);

    // Copy Ctor Time interval
    QMediaTimeInterval other(time);
    QVERIFY(other.isNormal() == time.isNormal());
    QVERIFY(other.start() == time.start());
    QVERIFY(other.end() == time.end());
    QVERIFY(other.contains(20) == time.contains(20));
    QVERIFY(other == time);
}

void tst_QMediaTimeRange::testIntervalContains()
{
    QMediaTimeInterval time(20,50);

    /* start() <= time <= end(). Returns true if the time interval contains the specified time. */
    QVERIFY(!time.contains(10));
    QVERIFY(time.contains(20));
    QVERIFY(time.contains(30));
    QVERIFY(time.contains(50));
    QVERIFY(!time.contains(60));

    QMediaTimeInterval x(20, 10); // denormal

    // Check denormal ranges
    QVERIFY(!x.contains(5));
    QVERIFY(x.contains(10));
    QVERIFY(x.contains(15));
    QVERIFY(x.contains(20));
    QVERIFY(!x.contains(25));

    QMediaTimeInterval y = x.normalized();
    QVERIFY(!y.contains(5));
    QVERIFY(y.contains(10));
    QVERIFY(y.contains(15));
    QVERIFY(y.contains(20));
    QVERIFY(!y.contains(25));
}

void tst_QMediaTimeRange::testCtor()
{
    // Default Ctor
    QMediaTimeRange a;
    QVERIFY(a.isEmpty());

    // (qint, qint) Ctor
    QMediaTimeRange b(10, 20);

    QVERIFY(!b.isEmpty());
    QVERIFY(b.isContinuous());
    QVERIFY(b.earliestTime() == 10);
    QVERIFY(b.latestTime() == 20);

    // Interval Ctor
    QMediaTimeRange c(QMediaTimeInterval(30, 40));

    QVERIFY(!c.isEmpty());
    QVERIFY(c.isContinuous());
    QVERIFY(c.earliestTime() == 30);
    QVERIFY(c.latestTime() == 40);

    // Abnormal Interval Ctor
    QMediaTimeRange d(QMediaTimeInterval(20, 10));

    QVERIFY(d.isEmpty());

    // Copy Ctor
    QMediaTimeRange e(b);

    QVERIFY(!e.isEmpty());
    QVERIFY(e.isContinuous());
    QVERIFY(e.earliestTime() == 10);
    QVERIFY(e.latestTime() == 20);

    QVERIFY(e == b);
}

void tst_QMediaTimeRange::testGetters()
{
    QMediaTimeRange x;

    // isEmpty
    QVERIFY(x.isEmpty());

    x.addInterval(10, 20);

    // isEmpty + isContinuous
    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());

    x.addInterval(30, 40);

    // isEmpty + isContinuous + intervals + start + end
    QVERIFY(!x.isEmpty());
    QVERIFY(!x.isContinuous());
    QVERIFY(x.intervals().count() == 2);
    QVERIFY(x.intervals()[0].start() == 10);
    QVERIFY(x.intervals()[0].end() == 20);
    QVERIFY(x.intervals()[1].start() == 30);
    QVERIFY(x.intervals()[1].end() == 40);
}

void tst_QMediaTimeRange::testAssignment()
{
    QMediaTimeRange x;

    // Range Assignment
    x = QMediaTimeRange(10, 20);

    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());
    QVERIFY(x.earliestTime() == 10);
    QVERIFY(x.latestTime() == 20);

    // Interval Assignment
    x = QMediaTimeInterval(30, 40);

    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());
    QVERIFY(x.earliestTime() == 30);
    QVERIFY(x.latestTime() == 40);

    // Shared Data Check
    QMediaTimeRange y;

    y = x;
    y.addInterval(10, 20);

    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());
    QVERIFY(x.earliestTime() == 30);
    QVERIFY(x.latestTime() == 40);
}

void tst_QMediaTimeRange::testIntervalNormalize()
{
    QMediaTimeInterval x(20, 10);

    QVERIFY(!x.isNormal());
    QVERIFY(x.start() == 20);
    QVERIFY(x.end() == 10);

    QMediaTimeInterval y = x.normalized();

    QVERIFY(y.isNormal());
    QVERIFY(y.start() == 10);
    QVERIFY(y.end() == 20);
    QVERIFY(x != y);
}

void tst_QMediaTimeRange::testIntervalTranslate()
{
    QMediaTimeInterval x(10, 20);
    x = x.translated(10);

    QVERIFY(x.start() == 20);
    QVERIFY(x.end() == 30);

    /* verifying the backward through time with a negative offset.*/
    x = x.translated(-10);

    QVERIFY(x.start() == 10);
    QVERIFY(x.end() == 20);
}

void tst_QMediaTimeRange::testEarliestLatest()
{
    // Test over a single interval
    QMediaTimeRange x(30, 40);

    QVERIFY(x.earliestTime() == 30);
    QVERIFY(x.latestTime() == 40);

    // Test over multiple intervals
    x.addInterval(50, 60);

    QVERIFY(x.earliestTime() == 30);
    QVERIFY(x.latestTime() == 60);
}

void tst_QMediaTimeRange::testContains()
{
    // Test over a single interval
    QMediaTimeRange x(10, 20);

    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());
    QVERIFY(x.contains(15));
    QVERIFY(x.contains(10));
    QVERIFY(x.contains(20));
    QVERIFY(!x.contains(25));

    // Test over multiple intervals
    x.addInterval(40, 50);

    QVERIFY(!x.isEmpty());
    QVERIFY(!x.isContinuous());
    QVERIFY(x.contains(15));
    QVERIFY(x.contains(45));
    QVERIFY(!x.contains(30));

    // Test over a concrete interval
    QMediaTimeInterval y(10, 20);
    QVERIFY(y.contains(15));
    QVERIFY(y.contains(10));
    QVERIFY(y.contains(20));
    QVERIFY(!y.contains(25));
}

void tst_QMediaTimeRange::testAddInterval()
{
    // All intervals Overlap
    QMediaTimeRange x;
    x.addInterval(10, 40);
    x.addInterval(30, 50);
    x.addInterval(20, 60);

    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());
    QVERIFY(x.earliestTime() == 10);
    QVERIFY(x.latestTime() == 60);

    // 1 adjacent interval, 1 encompassed interval
    x = QMediaTimeRange();
    x.addInterval(10, 40);
    x.addInterval(20, 30);
    x.addInterval(41, 50);

    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());
    QVERIFY(x.earliestTime() == 10);
    QVERIFY(x.latestTime() == 50);

    // 1 overlapping interval, 1 disjoint interval
    x = QMediaTimeRange();
    x.addInterval(10, 30);
    x.addInterval(20, 40);
    x.addInterval(50, 60);

    QVERIFY(!x.isEmpty());
    QVERIFY(!x.isContinuous());
    QVERIFY(x.intervals().count() == 2);
    QVERIFY(x.intervals()[0].start() == 10);
    QVERIFY(x.intervals()[0].end() == 40);
    QVERIFY(x.intervals()[1].start() == 50);
    QVERIFY(x.intervals()[1].end() == 60);

    // Identical Add
    x = QMediaTimeRange();
    x.addInterval(10, 20);
    x.addInterval(10, 20);

    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());
    QVERIFY(x.earliestTime() == 10);
    QVERIFY(x.latestTime() == 20);

    // Multi-Merge
    x = QMediaTimeRange();
    x.addInterval(10, 20);
    x.addInterval(30, 40);
    x.addInterval(50, 60);
    x.addInterval(15, 55);

    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());
    QVERIFY(x.earliestTime() == 10);
    QVERIFY(x.latestTime() == 60);

    // Interval Parameter - All intervals Overlap
    x = QMediaTimeRange();
    x.addInterval(QMediaTimeInterval(10, 40));
    x.addInterval(QMediaTimeInterval(30, 50));
    x.addInterval(QMediaTimeInterval(20, 60));

    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());
    QVERIFY(x.earliestTime() == 10);
    QVERIFY(x.latestTime() == 60);

    // Interval Parameter - Abnormal Interval
    x = QMediaTimeRange();
    x.addInterval(QMediaTimeInterval(20, 10));

    QVERIFY(x.isEmpty());
}

void tst_QMediaTimeRange::testAddTimeRange()
{
    // Add Time Range uses Add Interval internally,
    // so in this test the focus is on combinations of number
    // of intervals added, rather than the different types of
    // merges which can occur.
    QMediaTimeRange a, b;

    // Add Single into Single
    a = QMediaTimeRange(10, 30);
    b = QMediaTimeRange(20, 40);

    b.addTimeRange(a);

    QVERIFY(!b.isEmpty());
    QVERIFY(b.isContinuous());
    QVERIFY(b.earliestTime() == 10);
    QVERIFY(b.latestTime() == 40);

    // Add Multiple into Single
    a = QMediaTimeRange();
    a.addInterval(10, 30);
    a.addInterval(40, 60);

    b = QMediaTimeRange(20, 50);

    b.addTimeRange(a);

    QVERIFY(!b.isEmpty());
    QVERIFY(b.isContinuous());
    QVERIFY(b.earliestTime() == 10);
    QVERIFY(b.latestTime() == 60);

    // Add Single into Multiple
    a = QMediaTimeRange(20, 50);

    b = QMediaTimeRange();
    b.addInterval(10, 30);
    b.addInterval(40, 60);

    b.addTimeRange(a);

    QVERIFY(!b.isEmpty());
    QVERIFY(b.isContinuous());
    QVERIFY(b.earliestTime() == 10);
    QVERIFY(b.latestTime() == 60);

    // Add Multiple into Multiple
    a = QMediaTimeRange();
    a.addInterval(10, 30);
    a.addInterval(40, 70);
    a.addInterval(80, 100);

    b = QMediaTimeRange();
    b.addInterval(20, 50);
    b.addInterval(60, 90);

    b.addTimeRange(a);

    QVERIFY(!b.isEmpty());
    QVERIFY(b.isContinuous());
    QVERIFY(b.earliestTime() == 10);
    QVERIFY(b.latestTime() == 100);

    // Add Nothing to Single
    a = QMediaTimeRange();
    b = QMediaTimeRange(10, 20);

    b.addTimeRange(a);

    QVERIFY(!b.isEmpty());
    QVERIFY(b.isContinuous());
    QVERIFY(b.earliestTime() == 10);
    QVERIFY(b.latestTime() == 20);

    // Add Single to Nothing
    a = QMediaTimeRange(10, 20);
    b = QMediaTimeRange();

    b.addTimeRange(a);

    QVERIFY(!b.isEmpty());
    QVERIFY(b.isContinuous());
    QVERIFY(b.earliestTime() == 10);
    QVERIFY(b.latestTime() == 20);

    // Add Nothing to Nothing
    a = QMediaTimeRange();
    b = QMediaTimeRange();

    b.addTimeRange(a);

    QVERIFY(b.isEmpty());
}

void tst_QMediaTimeRange::testRemoveInterval()
{
    // Removing an interval, causing a split
    QMediaTimeRange x;
    x.addInterval(10, 50);
    x.removeInterval(20, 40);

    QVERIFY(!x.isEmpty());
    QVERIFY(!x.isContinuous());
    QVERIFY(x.intervals().count() == 2);
    QVERIFY(x.intervals()[0].start() == 10);
    QVERIFY(x.intervals()[0].end() == 19);
    QVERIFY(x.intervals()[1].start() == 41);
    QVERIFY(x.intervals()[1].end() == 50);

    // Removing an interval, causing a deletion
    x = QMediaTimeRange();
    x.addInterval(20, 30);
    x.removeInterval(10, 40);

    QVERIFY(x.isEmpty());

    // Removing an interval, causing a tail trim
    x = QMediaTimeRange();
    x.addInterval(20, 40);
    x.removeInterval(30, 50);

    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());
    QVERIFY(x.earliestTime() == 20);
    QVERIFY(x.latestTime() == 29);

    // Removing an interval, causing a head trim
    x = QMediaTimeRange();
    x.addInterval(20, 40);
    x.removeInterval(10, 30);

    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());
    QVERIFY(x.earliestTime() == 31);
    QVERIFY(x.latestTime() == 40);

    // Identical Remove
    x = QMediaTimeRange();
    x.addInterval(10, 20);
    x.removeInterval(10, 20);

    QVERIFY(x.isEmpty());

    // Multi-Trim
    x = QMediaTimeRange();
    x.addInterval(10, 20);
    x.addInterval(30, 40);
    x.removeInterval(15, 35);

    QVERIFY(!x.isEmpty());
    QVERIFY(!x.isContinuous());
    QVERIFY(x.intervals().count() == 2);
    QVERIFY(x.intervals()[0].start() == 10);
    QVERIFY(x.intervals()[0].end() == 14);
    QVERIFY(x.intervals()[1].start() == 36);
    QVERIFY(x.intervals()[1].end() == 40);

    // Multi-Delete
    x = QMediaTimeRange();
    x.addInterval(10, 20);
    x.addInterval(30, 40);
    x.addInterval(50, 60);
    x.removeInterval(10, 60);

    QVERIFY(x.isEmpty());

    // Interval Parameter - Removing an interval, causing a split
    x = QMediaTimeRange();
    x.addInterval(10, 50);
    x.removeInterval(QMediaTimeInterval(20, 40));

    QVERIFY(!x.isEmpty());
    QVERIFY(!x.isContinuous());
    QVERIFY(x.intervals().count() == 2);
    QVERIFY(x.intervals()[0].start() == 10);
    QVERIFY(x.intervals()[0].end() == 19);
    QVERIFY(x.intervals()[1].start() == 41);
    QVERIFY(x.intervals()[1].end() == 50);

    // Interval Parameter - Abnormal Interval
    x = QMediaTimeRange();
    x.addInterval(10, 40);
    x.removeInterval(QMediaTimeInterval(30, 20));

    QVERIFY(!x.isEmpty());
    QVERIFY(x.isContinuous());
    QVERIFY(x.earliestTime() == 10);
    QVERIFY(x.latestTime() == 40);
}

void tst_QMediaTimeRange::testRemoveTimeRange()
{
    // Remove Time Range uses Remove Interval internally,
    // so in this test the focus is on combinations of number
    // of intervals removed, rather than the different types of
    // deletions which can occur.
    QMediaTimeRange a, b;

    // Remove Single from Single
    a = QMediaTimeRange(10, 30);
    b = QMediaTimeRange(20, 40);

    b.removeTimeRange(a);

    QVERIFY(!b.isEmpty());
    QVERIFY(b.isContinuous());
    QVERIFY(b.earliestTime() == 31);
    QVERIFY(b.latestTime() == 40);

    // Remove Multiple from Single
    a = QMediaTimeRange();
    a.addInterval(10, 30);
    a.addInterval(40, 60);

    b = QMediaTimeRange(20, 50);

    b.removeTimeRange(a);

    QVERIFY(!b.isEmpty());
    QVERIFY(b.isContinuous());
    QVERIFY(b.earliestTime() == 31);
    QVERIFY(b.latestTime() == 39);

    // Remove Single from Multiple
    a = QMediaTimeRange(20, 50);

    b = QMediaTimeRange();
    b.addInterval(10, 30);
    b.addInterval(40, 60);

    b.removeTimeRange(a);

    QVERIFY(!b.isEmpty());
    QVERIFY(!b.isContinuous());
    QVERIFY(b.intervals().count() == 2);
    QVERIFY(b.intervals()[0].start() == 10);
    QVERIFY(b.intervals()[0].end() == 19);
    QVERIFY(b.intervals()[1].start() == 51);
    QVERIFY(b.intervals()[1].end() == 60);

    // Remove Multiple from Multiple
    a = QMediaTimeRange();
    a.addInterval(20, 50);
    a.addInterval(50, 90);


    b = QMediaTimeRange();
    b.addInterval(10, 30);
    b.addInterval(40, 70);
    b.addInterval(80, 100);

    b.removeTimeRange(a);

    QVERIFY(!b.isEmpty());
    QVERIFY(!b.isContinuous());
    QVERIFY(b.intervals().count() == 2);
    QVERIFY(b.intervals()[0].start() == 10);
    QVERIFY(b.intervals()[0].end() == 19);
    QVERIFY(b.intervals()[1].start() == 91);
    QVERIFY(b.intervals()[1].end() == 100);

    // Remove Nothing from Single
    a = QMediaTimeRange();
    b = QMediaTimeRange(10, 20);

    b.removeTimeRange(a);

    QVERIFY(!b.isEmpty());
    QVERIFY(b.isContinuous());
    QVERIFY(b.earliestTime() == 10);
    QVERIFY(b.latestTime() == 20);

    // Remove Single from Nothing
    a = QMediaTimeRange(10, 20);
    b = QMediaTimeRange();

    b.removeTimeRange(a);

    QVERIFY(b.isEmpty());

    // Remove Nothing from Nothing
    a = QMediaTimeRange();
    b = QMediaTimeRange();

    b.removeTimeRange(a);

    QVERIFY(b.isEmpty());
}

void tst_QMediaTimeRange::testClear()
{
    QMediaTimeRange x;

    // Clear Nothing
    x.clear();

    QVERIFY(x.isEmpty());

    // Clear Single
    x = QMediaTimeRange(10, 20);
    x.clear();

    QVERIFY(x.isEmpty());

    // Clear Multiple
    x = QMediaTimeRange();
    x.addInterval(10, 20);
    x.addInterval(30, 40);
    x.clear();

    QVERIFY(x.isEmpty());
}

void tst_QMediaTimeRange::testComparisons()
{
    // Interval equality
    QVERIFY(QMediaTimeInterval(10, 20) == QMediaTimeInterval(10, 20));
    QVERIFY(QMediaTimeInterval(10, 20) != QMediaTimeInterval(10, 30));
    QVERIFY(!(QMediaTimeInterval(10, 20) != QMediaTimeInterval(10, 20)));
    QVERIFY(!(QMediaTimeInterval(10, 20) == QMediaTimeInterval(10, 30)));

    // Time range equality - Single Interval
    QMediaTimeRange a(10, 20), b(20, 30), c(10, 20);

    QVERIFY(a == c);
    QVERIFY(!(a == b));
    QVERIFY(a != b);
    QVERIFY(!(a != c));

    // Time Range Equality - Multiple Intervals
    QMediaTimeRange x, y, z;

    x.addInterval(10, 20);
    x.addInterval(30, 40);
    x.addInterval(50, 60);

    y.addInterval(10, 20);
    y.addInterval(35, 45);
    y.addInterval(50, 60);

    z.addInterval(10, 20);
    z.addInterval(30, 40);
    z.addInterval(50, 60);

    QVERIFY(x == z);
    QVERIFY(!(x == y));
    QVERIFY(x != y);
    QVERIFY(!(x != z));
}

void tst_QMediaTimeRange::testArithmetic()
{
    QMediaTimeRange a(10, 20), b(20, 30);

    // Test +=
    a += b;

    QVERIFY(a.isContinuous());
    QVERIFY(a.earliestTime() == 10);
    QVERIFY(a.latestTime() == 30);

    // Test -=
    a -= b;

    QVERIFY(a.isContinuous());
    QVERIFY(a.earliestTime() == 10);
    QVERIFY(a.latestTime() == 19);

    // Test += and -= on intervals
    a -= QMediaTimeInterval(10, 20);
    a += QMediaTimeInterval(40, 50);

    QVERIFY(a.isContinuous());
    QVERIFY(a.earliestTime() == 40);
    QVERIFY(a.latestTime() == 50);

    // Test Interval + Interval
    a = QMediaTimeInterval(10, 20) + QMediaTimeInterval(20, 30);
    QVERIFY(a.isContinuous());
    QVERIFY(a.earliestTime() == 10);
    QVERIFY(a.latestTime() == 30);

    // Test Range + Interval
    a = a + QMediaTimeInterval(30, 40);
    QVERIFY(a.isContinuous());
    QVERIFY(a.earliestTime() == 10);
    QVERIFY(a.latestTime() == 40);

    // Test Interval + Range
    a = QMediaTimeInterval(40, 50) + a;
    QVERIFY(a.isContinuous());
    QVERIFY(a.earliestTime() == 10);
    QVERIFY(a.latestTime() == 50);

    // Test Range + Range
    a = a + QMediaTimeRange(50, 60);
    QVERIFY(a.isContinuous());
    QVERIFY(a.earliestTime() == 10);
    QVERIFY(a.latestTime() == 60);

    // Test Range - Interval
    a = a - QMediaTimeInterval(50, 60);
    QVERIFY(a.isContinuous());
    QVERIFY(a.earliestTime() == 10);
    QVERIFY(a.latestTime() == 49);

    // Test Range - Range
    a = a - QMediaTimeRange(40, 50);
    QVERIFY(a.isContinuous());
    QVERIFY(a.earliestTime() == 10);
    QVERIFY(a.latestTime() == 39);

    // Test Interval - Range
    b = QMediaTimeInterval(0, 20) - a;
    QVERIFY(b.isContinuous());
    QVERIFY(b.earliestTime() == 0);
    QVERIFY(b.latestTime() == 9);

    // Test Interval - Interval
    a = QMediaTimeInterval(10, 20) - QMediaTimeInterval(15, 30);
    QVERIFY(a.isContinuous());
    QVERIFY(a.earliestTime() == 10);
    QVERIFY(a.latestTime() == 14);
}

QTEST_MAIN(tst_QMediaTimeRange)

#include "tst_qmediatimerange.moc"
