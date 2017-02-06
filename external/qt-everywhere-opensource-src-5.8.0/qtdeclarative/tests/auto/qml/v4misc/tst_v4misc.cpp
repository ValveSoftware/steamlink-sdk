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

#include <qtest.h>

#include <private/qv4ssa_p.h>

class tst_v4misc: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void rangeSplitting_1();
    void rangeSplitting_2();
    void rangeSplitting_3();
};

QT_BEGIN_NAMESPACE
// Avoid QHash randomization so that the temp numbering is stable.
extern Q_CORE_EXPORT QBasicAtomicInt qt_qhash_seed; // from qhash.cpp
QT_END_NAMESPACE

using namespace QT_PREPEND_NAMESPACE(QV4::IR);

void tst_v4misc::initTestCase()
{
    qt_qhash_seed.store(0);
    QCOMPARE(qt_qhash_seed.load(), 0);
}

// split between two ranges
void tst_v4misc::rangeSplitting_1()
{
    LifeTimeInterval interval;
    interval.addRange(59, 59);
    interval.addRange(61, 62);
    interval.addRange(64, 64);
    interval.addRange(69, 71);
    interval.validate();
    QCOMPARE(interval.end(), 71);

    LifeTimeInterval newInterval = interval.split(66, 70);
    interval.validate();
    newInterval.validate();
    QVERIFY(newInterval.isSplitFromInterval());

    QCOMPARE(interval.ranges().size(), 3);
    QCOMPARE(interval.ranges()[0].start, 59);
    QCOMPARE(interval.ranges()[0].end, 59);
    QCOMPARE(interval.ranges()[1].start, 61);
    QCOMPARE(interval.ranges()[1].end, 62);
    QCOMPARE(interval.ranges()[2].start, 64);
    QCOMPARE(interval.ranges()[2].end, 64);
    QCOMPARE(interval.end(), 70);

    QCOMPARE(newInterval.ranges().size(), 1);
    QCOMPARE(newInterval.ranges()[0].start, 70);
    QCOMPARE(newInterval.ranges()[0].end, 71);
    QCOMPARE(newInterval.end(), 71);
}

// split in the middle of a range
void tst_v4misc::rangeSplitting_2()
{
    LifeTimeInterval interval;
    interval.addRange(59, 59);
    interval.addRange(61, 64);
    interval.addRange(69, 71);
    interval.validate();
    QCOMPARE(interval.end(), 71);

    LifeTimeInterval newInterval = interval.split(62, 64);
    interval.validate();
    newInterval.validate();
    QVERIFY(newInterval.isSplitFromInterval());

    QCOMPARE(interval.ranges().size(), 2);
    QCOMPARE(interval.ranges()[0].start, 59);
    QCOMPARE(interval.ranges()[0].end, 59);
    QCOMPARE(interval.ranges()[1].start, 61);
    QCOMPARE(interval.ranges()[1].end, 62);
    QCOMPARE(interval.end(), 64);

    QCOMPARE(newInterval.ranges().size(), 2);
    QCOMPARE(newInterval.ranges()[0].start, 64);
    QCOMPARE(newInterval.ranges()[0].end, 64);
    QCOMPARE(newInterval.ranges()[1].start, 69);
    QCOMPARE(newInterval.ranges()[1].end, 71);
    QCOMPARE(newInterval.end(), 71);
}

// split in the middle of a range, and let it never go back to active again
void tst_v4misc::rangeSplitting_3()
{
    LifeTimeInterval interval;
    interval.addRange(59, 59);
    interval.addRange(61, 64);
    interval.addRange(69, 71);
    interval.validate();
    QCOMPARE(interval.end(), 71);

    LifeTimeInterval newInterval = interval.split(64, LifeTimeInterval::InvalidPosition);
    interval.validate();
    newInterval.validate();
    QVERIFY(!newInterval.isValid());

    QCOMPARE(interval.ranges().size(), 2);
    QCOMPARE(interval.ranges()[0].start, 59);
    QCOMPARE(interval.ranges()[0].end, 59);
    QCOMPARE(interval.ranges()[1].start, 61);
    QCOMPARE(interval.ranges()[1].end, 64);
    QCOMPARE(interval.end(), 71);
}

QTEST_MAIN(tst_v4misc)

#include "tst_v4misc.moc"
