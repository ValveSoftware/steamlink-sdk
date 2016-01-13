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
#include <qtest.h>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <private/qdeclarativesmoothedanimation_p.h>
#include <private/qdeclarativerectangle_p.h>
#include <private/qdeclarativevaluetype_p.h>

class tst_qdeclarativesmoothedanimation : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativesmoothedanimation();

private slots:
    void defaultValues();
    void values();
    void disabled();
    void simpleAnimation();
    void valueSource();
    void behavior();
    void zeroDuration();

private:
    QDeclarativeEngine engine;
};

tst_qdeclarativesmoothedanimation::tst_qdeclarativesmoothedanimation()
{
}

void tst_qdeclarativesmoothedanimation::defaultValues()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/smoothedanimation1.qml"));
    QDeclarativeSmoothedAnimation *obj = qobject_cast<QDeclarativeSmoothedAnimation*>(c.create());

    QVERIFY(obj != 0);

    QCOMPARE(obj->to(), 0.);
    QCOMPARE(obj->velocity(), 200.);
    QCOMPARE(obj->duration(), -1);
    QCOMPARE(obj->maximumEasingTime(), -1);
    QCOMPARE(obj->reversingMode(), QDeclarativeSmoothedAnimation::Eased);

    delete obj;
}

void tst_qdeclarativesmoothedanimation::values()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/smoothedanimation2.qml"));
    QDeclarativeSmoothedAnimation *obj = qobject_cast<QDeclarativeSmoothedAnimation*>(c.create());

    QVERIFY(obj != 0);

    QCOMPARE(obj->to(), 10.);
    QCOMPARE(obj->velocity(), 200.);
    QCOMPARE(obj->duration(), 300);
    QCOMPARE(obj->maximumEasingTime(), -1);
    QCOMPARE(obj->reversingMode(), QDeclarativeSmoothedAnimation::Immediate);

    delete obj;
}

void tst_qdeclarativesmoothedanimation::disabled()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/smoothedanimation3.qml"));
    QDeclarativeSmoothedAnimation *obj = qobject_cast<QDeclarativeSmoothedAnimation*>(c.create());

    QVERIFY(obj != 0);

    QCOMPARE(obj->to(), 10.);
    QCOMPARE(obj->velocity(), 250.);
    QCOMPARE(obj->maximumEasingTime(), 150);
    QCOMPARE(obj->reversingMode(), QDeclarativeSmoothedAnimation::Sync);

    delete obj;
}

void tst_qdeclarativesmoothedanimation::simpleAnimation()
{
    QDeclarativeRectangle rect;
    QDeclarativeSmoothedAnimation animation;
    animation.setTarget(&rect);
    animation.setProperty("x");
    animation.setTo(200);
    animation.setDuration(250);
    QVERIFY(animation.target() == &rect);
    QVERIFY(animation.property() == "x");
    QVERIFY(animation.to() == 200);
    animation.start();
    QVERIFY(animation.isRunning());
    QTest::qWait(animation.duration());
    QTRY_COMPARE(rect.x(), qreal(200));

    rect.setX(0);
    animation.start();
    animation.pause();
    QVERIFY(animation.isRunning());
    QVERIFY(animation.isPaused());
    animation.setCurrentTime(125);
    QVERIFY(animation.currentTime() == 125);
    QCOMPARE(rect.x(), qreal(100));
}

void tst_qdeclarativesmoothedanimation::valueSource()
{
    QDeclarativeEngine engine;

    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/smoothedanimationValueSource.qml"));

    QDeclarativeRectangle *rect = qobject_cast<QDeclarativeRectangle*>(c.create());
    QVERIFY(rect);

    QDeclarativeRectangle *theRect = rect->findChild<QDeclarativeRectangle*>("theRect");
    QVERIFY(theRect);

    QDeclarativeSmoothedAnimation *easeX = rect->findChild<QDeclarativeSmoothedAnimation*>("easeX");
    QVERIFY(easeX);
    QVERIFY(easeX->isRunning());

    QDeclarativeSmoothedAnimation *easeY = rect->findChild<QDeclarativeSmoothedAnimation*>("easeY");
    QVERIFY(easeY);
    QVERIFY(easeY->isRunning());

    // XXX get the proper duration
    QTest::qWait(100);

    QTRY_VERIFY(!easeX->isRunning());
    QTRY_VERIFY(!easeY->isRunning());

    QTRY_COMPARE(theRect->x(), qreal(200));
    QTRY_COMPARE(theRect->y(), qreal(200));
}

void tst_qdeclarativesmoothedanimation::behavior()
{
    QDeclarativeEngine engine;

    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/smoothedanimationBehavior.qml"));

    QDeclarativeRectangle *rect = qobject_cast<QDeclarativeRectangle*>(c.create());
    QVERIFY(rect);

    QDeclarativeRectangle *theRect = rect->findChild<QDeclarativeRectangle*>("theRect");
    QVERIFY(theRect);

    QDeclarativeSmoothedAnimation *easeX = rect->findChild<QDeclarativeSmoothedAnimation*>("easeX");
    QVERIFY(easeX);

    QDeclarativeSmoothedAnimation *easeY = rect->findChild<QDeclarativeSmoothedAnimation*>("easeY");
    QVERIFY(easeY);

    // XXX get the proper duration
    QTest::qWait(400);

    QTRY_VERIFY(!easeX->isRunning());
    QTRY_VERIFY(!easeY->isRunning());

    QTRY_COMPARE(theRect->x(), qreal(200));
    QTRY_COMPARE(theRect->y(), qreal(200));
}

void tst_qdeclarativesmoothedanimation::zeroDuration()
{
    QDeclarativeEngine engine;

    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/smoothedanimationZeroDuration.qml"));

    QDeclarativeRectangle *rect = qobject_cast<QDeclarativeRectangle*>(c.create());
    QVERIFY(rect);

    QDeclarativeRectangle *theRect = rect->findChild<QDeclarativeRectangle*>("theRect");
    QVERIFY(theRect);

    QDeclarativeSmoothedAnimation *easeX = rect->findChild<QDeclarativeSmoothedAnimation*>("easeX");
    QVERIFY(easeX);
    QVERIFY(easeX->isRunning());

    QTRY_VERIFY(!easeX->isRunning());
    QTRY_COMPARE(theRect->x(), qreal(200));
}

QTEST_MAIN(tst_qdeclarativesmoothedanimation)

#include "tst_qdeclarativesmoothedanimation.moc"
