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

class tst_qquickcustomparticle : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickcustomparticle() {}

private slots:
    void initTestCase();
    void test_basic();
    void test_deleteSourceItem();
};

void tst_qquickcustomparticle::initTestCase()
{
    QQmlDataTest::initTestCase();
    QUnifiedTimer::instance()->setConsistentTiming(true);
}

void tst_qquickcustomparticle::test_basic()
{
    QQuickView* view = createView(testFileUrl("basic.qml"), 600);
    QVERIFY(view);
    QQuickParticleSystem* system = view->rootObject()->findChild<QQuickParticleSystem*>("system");
    ensureAnimTime(600, system->m_animation);

    bool oneNonZero = false;
    QVERIFY(extremelyFuzzyCompare(system->groupData[0]->size(), 500, 10));
    for (QQuickParticleData *d : qAsConst(system->groupData[0]->data)) {
        if (d->t == -1)
            continue; //Particle data unused

        QCOMPARE(d->x, 0.f);
        QCOMPARE(d->y, 0.f);
        QCOMPARE(d->vx, 0.f);
        QCOMPARE(d->vy, 0.f);
        QCOMPARE(d->ax, 0.f);
        QCOMPARE(d->ay, 0.f);
        QCOMPARE(d->lifeSpan, 0.5f);
        QCOMPARE(d->size, 32.f);
        QCOMPARE(d->endSize, 32.f);
        QVERIFY(myFuzzyLEQ(d->t, ((qreal)system->timeInt/1000.0)));
        QVERIFY(d->r >= 0.0 && d->r <= 1.0);
        if (d->r != 0.0 )
            oneNonZero = true;
    }
    delete view;
    QVERIFY(oneNonZero);//Zero is a valid value, but it also needs to be set to a random number
}

void tst_qquickcustomparticle::test_deleteSourceItem()
{
    // purely to ensure that deleting the sourceItem of a shader doesn't cause a crash
    QQuickView* view = createView(testFileUrl("deleteSourceItem.qml"), 600);
    QVERIFY(view);
    QObject *obj = view->rootObject();
    QVERIFY(obj);
    QQuickParticleSystem* system = view->rootObject()->findChild<QQuickParticleSystem*>("system");
    ensureAnimTime(200, system->m_animation);
    QMetaObject::invokeMethod(obj, "setDeletedSourceItem");
    ensureAnimTime(200, system->m_animation);
    delete view;
}

QTEST_MAIN(tst_qquickcustomparticle);

#include "tst_qquickcustomparticle.moc"
