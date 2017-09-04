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

#include <qhashfunctions.h>
#include <qtest.h>

#define V4_AUTOTEST
#include <private/qv4ssa_p.h>

class tst_v4misc: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void rangeSplitting_1();
    void rangeSplitting_2();
    void rangeSplitting_3();

    void moveMapping_1();
    void moveMapping_2();
};

using namespace QT_PREPEND_NAMESPACE(QV4::IR);

void tst_v4misc::initTestCase()
{
    qSetGlobalQHashSeed(0);
    QCOMPARE(qGlobalQHashSeed(), 0);
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

void tst_v4misc::moveMapping_1()
{
    Temp fp2(DoubleType, Temp::PhysicalRegister, 2);
    Temp fp3(DoubleType, Temp::PhysicalRegister, 3);
    Temp fp4(DoubleType, Temp::PhysicalRegister, 4);
    Temp fp5(DoubleType, Temp::PhysicalRegister, 5);

    MoveMapping mapping;
    mapping.add(&fp2, &fp3);
    mapping.add(&fp2, &fp4);
    mapping.add(&fp2, &fp5);
    mapping.add(&fp3, &fp2);

    mapping.order();
//    mapping.dump();

    QCOMPARE(mapping._moves.size(), 3);
    QVERIFY(mapping._moves.contains(MoveMapping::Move(&fp2, &fp4, false)));
    QVERIFY(mapping._moves.contains(MoveMapping::Move(&fp2, &fp5, false)));
    QVERIFY(mapping._moves.last() == MoveMapping::Move(&fp2, &fp3, true) ||
            mapping._moves.last() == MoveMapping::Move(&fp3, &fp2, true));
}

void tst_v4misc::moveMapping_2()
{
    Temp fp1(DoubleType, Temp::PhysicalRegister, 1);
    Temp fp2(DoubleType, Temp::PhysicalRegister, 2);
    Temp fp3(DoubleType, Temp::PhysicalRegister, 3);
    Temp fp4(DoubleType, Temp::PhysicalRegister, 4);
    Temp fp5(DoubleType, Temp::PhysicalRegister, 5);
    Temp fp6(DoubleType, Temp::PhysicalRegister, 6);
    Temp fp7(DoubleType, Temp::PhysicalRegister, 7);
    Temp fp8(DoubleType, Temp::PhysicalRegister, 8);
    Temp fp9(DoubleType, Temp::PhysicalRegister, 9);
    Temp fp10(DoubleType, Temp::PhysicalRegister, 10);
    Temp fp11(DoubleType, Temp::PhysicalRegister, 11);
    Temp fp12(DoubleType, Temp::PhysicalRegister, 12);
    Temp fp13(DoubleType, Temp::PhysicalRegister, 13);

    MoveMapping mapping;
    mapping.add(&fp2, &fp1);
    mapping.add(&fp2, &fp3);
    mapping.add(&fp3, &fp2);
    mapping.add(&fp3, &fp4);

    mapping.add(&fp9, &fp8);
    mapping.add(&fp8, &fp7);
    mapping.add(&fp7, &fp6);
    mapping.add(&fp7, &fp5);

    mapping.add(&fp10, &fp11);
    mapping.add(&fp11, &fp12);
    mapping.add(&fp12, &fp13);
    mapping.add(&fp13, &fp10);

    mapping.order();
//    mapping.dump();

    QCOMPARE(mapping._moves.size(), 10);

    QVERIFY(mapping._moves.contains(MoveMapping::Move(&fp2, &fp1, false)));
    QVERIFY(mapping._moves.contains(MoveMapping::Move(&fp3, &fp4, false)));
    QVERIFY(mapping._moves.contains(MoveMapping::Move(&fp7, &fp6, false)));
    QVERIFY(mapping._moves.contains(MoveMapping::Move(&fp7, &fp5, false)));
    QVERIFY(mapping._moves.contains(MoveMapping::Move(&fp8, &fp7, false)));
    QVERIFY(mapping._moves.contains(MoveMapping::Move(&fp9, &fp8, false)));

    QVERIFY(mapping._moves.contains(MoveMapping::Move(&fp2, &fp3, true)) ||
            mapping._moves.contains(MoveMapping::Move(&fp3, &fp2, true)));
    QVERIFY(mapping._moves.contains(MoveMapping::Move(&fp10, &fp13, true)) ||
            mapping._moves.contains(MoveMapping::Move(&fp13, &fp10, true)));
    QVERIFY(mapping._moves.contains(MoveMapping::Move(&fp12, &fp13, true)) ||
            mapping._moves.contains(MoveMapping::Move(&fp13, &fp12, true)));
    QVERIFY(mapping._moves.contains(MoveMapping::Move(&fp12, &fp11, true)) ||
            mapping._moves.contains(MoveMapping::Move(&fp11, &fp12, true)));

    QVERIFY(!mapping._moves.at(0).needsSwap);
    QVERIFY(!mapping._moves.at(1).needsSwap);
    QVERIFY(!mapping._moves.at(2).needsSwap);
    QVERIFY(!mapping._moves.at(3).needsSwap);
    QVERIFY(!mapping._moves.at(4).needsSwap);
    QVERIFY(!mapping._moves.at(5).needsSwap);
    QVERIFY(mapping._moves.at(6).needsSwap);
    QVERIFY(mapping._moves.at(7).needsSwap);
    QVERIFY(mapping._moves.at(8).needsSwap);
    QVERIFY(mapping._moves.at(9).needsSwap);
}

QTEST_MAIN(tst_v4misc)

#include "tst_v4misc.moc"
