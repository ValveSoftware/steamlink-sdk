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

#include <QtTest/QtTest>
#include "../shared/particlestestsshared.h"
#include <private/qquickparticlesystem_p.h>
#include <private/qabstractanimation_p.h>

#include "../../shared/util.h"

class tst_qquickcustomaffector : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickcustomaffector() {}

private slots:
    void initTestCase();
    void test_basic();
    void test_move();
    void test_affectedSignal();
};

void tst_qquickcustomaffector::initTestCase()
{
    QQmlDataTest::initTestCase();
    QUnifiedTimer::instance()->setConsistentTiming(true);
}

void tst_qquickcustomaffector::test_basic()
{
    QQuickView* view = createView(testFileUrl("basic.qml"), 600);
    QQuickParticleSystem* system = view->rootObject()->findChild<QQuickParticleSystem*>("system");
    ensureAnimTime(600, system->m_animation);

    QVERIFY(extremelyFuzzyCompare(system->groupData[0]->size(), 500, 10));
    for (QQuickParticleData *d : qAsConst(system->groupData[0]->data)) {
        if (d->t == -1)
            continue; //Particle data unused
        //in CI the whole simulation often happens at once, so dead particles end up missing out
        if (!d->stillAlive(system))
            continue; //parameters no longer get set once you die

        QCOMPARE(d->x, 100.f);
        QCOMPARE(d->y, 100.f);
        QCOMPARE(d->vx, 100.f);
        QCOMPARE(d->vy, 100.f);
        QCOMPARE(d->ax, 100.f);
        QCOMPARE(d->ay, 100.f);
        QCOMPARE(d->lifeSpan, 0.5f);
        QCOMPARE(d->size, 100.f);
        QCOMPARE(d->endSize, 100.f);
        QCOMPARE(d->autoRotate, 1.f);
        QCOMPARE(d->color.r, (uchar)0);
        QCOMPARE(d->color.g, (uchar)255);
        QCOMPARE(d->color.b, (uchar)0);
        QCOMPARE(d->color.a, (uchar)0);
        QVERIFY(myFuzzyLEQ(d->t, ((qreal)system->timeInt/1000.0)));
    }
    delete view;
}

void tst_qquickcustomaffector::test_move()
{
    QQuickView* view = createView(testFileUrl("move.qml"), 600);
    QQuickParticleSystem* system = view->rootObject()->findChild<QQuickParticleSystem*>("system");
    ensureAnimTime(600, system->m_animation);

    QVERIFY(extremelyFuzzyCompare(system->groupData[0]->size(), 500, 10));
    for (QQuickParticleData *d : qAsConst(system->groupData[0]->data)) {
        if (d->t == -1)
            continue; //Particle data unused
        if (!d->stillAlive(system))
            continue; //parameters no longer get set once you die

        QVERIFY(myFuzzyCompare(d->curX(system), 50.0));
        QVERIFY(myFuzzyCompare(d->curY(system), 50.0));
        QVERIFY(myFuzzyCompare(d->curVX(system), 50.0));
        QVERIFY(myFuzzyCompare(d->curVY(system), 50.0));
        QVERIFY(myFuzzyCompare(d->curAX(), 50.0));
        QVERIFY(myFuzzyCompare(d->curAY(), 50.0));
        QCOMPARE(d->lifeSpan, 0.5f);
        QCOMPARE(d->size, 32.f);
        QCOMPARE(d->endSize, 32.f);
        QVERIFY(myFuzzyLEQ(d->t, ((qreal)system->timeInt/1000.0)));
    }
    delete view;
}

void tst_qquickcustomaffector::test_affectedSignal()
{
    QQuickView* view = createView(testFileUrl("affectedSignal.qml"), 600);
    QQuickParticleSystem* system = view->rootObject()->findChild<QQuickParticleSystem*>("system");
    ensureAnimTime(600, system->m_animation);

    QCOMPARE(system->property("resultX1").toInt(), 0);
    QCOMPARE(system->property("resultY1").toInt(), 100);
    QCOMPARE(system->property("resultX2").toInt(), 1234);
    QCOMPARE(system->property("resultY2").toInt(), 1234);
    delete view;
}

QTEST_MAIN(tst_qquickcustomaffector);

#include "tst_qquickcustomaffector.moc"
