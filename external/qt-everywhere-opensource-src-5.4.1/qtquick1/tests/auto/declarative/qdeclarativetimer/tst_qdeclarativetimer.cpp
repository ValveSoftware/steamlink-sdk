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
#include <QtTest/QSignalSpy>
#include <qtest.h>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <private/qdeclarativetimer_p.h>
#include <QtDeclarative/qdeclarativeitem.h>
#include <QDebug>
#include <QtCore/QPauseAnimation>
#include <private/qabstractanimation_p.h>

void consistentWait(int ms)
{
    //Use animations for timing, because we enabled consistentTiming
    //This function will qWait for >= ms worth of consistent timing to elapse
    QPauseAnimation waitTimer(ms);
    waitTimer.start();
    while (waitTimer.state() == QAbstractAnimation::Running)
        QTest::qWait(20);
}

class tst_qdeclarativetimer : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativetimer();

private slots:
    void initTestCase();
    void notRepeating();
    void notRepeatingStart();
    void repeat();
    void noTriggerIfNotRunning();
    void triggeredOnStart();
    void triggeredOnStartRepeat();
    void changeDuration();
    void restart();
    void parentProperty();
};

class TimerHelper : public QObject
{
    Q_OBJECT
public:
    TimerHelper() : QObject(), count(0)
    {
    }

    int count;

public slots:
    void timeout() {
        ++count;
    }
};

tst_qdeclarativetimer::tst_qdeclarativetimer()
{
}

void tst_qdeclarativetimer::initTestCase()
{
    QUnifiedTimer::instance()->setConsistentTiming(true);
}

void tst_qdeclarativetimer::notRepeating()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(QByteArray("import QtQuick 1.0\nTimer { interval: 100; running: true }"), QUrl::fromLocalFile(""));
    QDeclarativeTimer *timer = qobject_cast<QDeclarativeTimer*>(component.create());
    QVERIFY(timer != 0);
    QVERIFY(timer->isRunning());
    QVERIFY(!timer->isRepeating());
    QCOMPARE(timer->interval(), 100);

    TimerHelper helper;
    connect(timer, SIGNAL(triggered()), &helper, SLOT(timeout()));


    consistentWait(200);
    QCOMPARE(helper.count, 1);
    consistentWait(200);
    QCOMPARE(helper.count, 1);
    QVERIFY(timer->isRunning() == false);
}

void tst_qdeclarativetimer::notRepeatingStart()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(QByteArray("import QtQuick 1.0\nTimer { interval: 100 }"), QUrl::fromLocalFile(""));
    QDeclarativeTimer *timer = qobject_cast<QDeclarativeTimer*>(component.create());
    QVERIFY(timer != 0);
    QVERIFY(!timer->isRunning());

    TimerHelper helper;
    connect(timer, SIGNAL(triggered()), &helper, SLOT(timeout()));

    consistentWait(200);
    QCOMPARE(helper.count, 0);

    timer->start();
    consistentWait(200);
    QCOMPARE(helper.count, 1);
    consistentWait(200);
    QCOMPARE(helper.count, 1);
    QVERIFY(timer->isRunning() == false);

    delete timer;
}

void tst_qdeclarativetimer::repeat()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(QByteArray("import QtQuick 1.0\nTimer { interval: 100; repeat: true; running: true }"), QUrl::fromLocalFile(""));
    QDeclarativeTimer *timer = qobject_cast<QDeclarativeTimer*>(component.create());
    QVERIFY(timer != 0);

    TimerHelper helper;
    connect(timer, SIGNAL(triggered()), &helper, SLOT(timeout()));
    QCOMPARE(helper.count, 0);

    consistentWait(200);
    QVERIFY(helper.count > 0);
    int oldCount = helper.count;

    consistentWait(200);
    QVERIFY(helper.count > oldCount);
    QVERIFY(timer->isRunning());

    oldCount = helper.count;
    timer->stop();

    consistentWait(200);
    QVERIFY(helper.count == oldCount);
    QVERIFY(timer->isRunning() == false);

    QSignalSpy spy(timer, SIGNAL(repeatChanged()));

    timer->setRepeating(false);
    QVERIFY(!timer->isRepeating());
    QCOMPARE(spy.count(),1);

    timer->setRepeating(false);
    QCOMPARE(spy.count(),1);

    timer->setRepeating(true);
    QCOMPARE(spy.count(),2);

    delete timer;
}

void tst_qdeclarativetimer::triggeredOnStart()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(QByteArray("import QtQuick 1.0\nTimer { interval: 100; running: true; triggeredOnStart: true }"), QUrl::fromLocalFile(""));
    QDeclarativeTimer *timer = qobject_cast<QDeclarativeTimer*>(component.create());
    QVERIFY(timer != 0);
    QVERIFY(timer->triggeredOnStart());

    TimerHelper helper;
    connect(timer, SIGNAL(triggered()), &helper, SLOT(timeout()));
    consistentWait(1);
    QCOMPARE(helper.count, 1);
    consistentWait(200);
    QCOMPARE(helper.count, 2);
    consistentWait(200);
    QCOMPARE(helper.count, 2);
    QVERIFY(timer->isRunning() == false);

    QSignalSpy spy(timer, SIGNAL(triggeredOnStartChanged()));

    timer->setTriggeredOnStart(false);
    QVERIFY(!timer->triggeredOnStart());
    QCOMPARE(spy.count(),1);

    timer->setTriggeredOnStart(false);
    QCOMPARE(spy.count(),1);

    timer->setTriggeredOnStart(true);
    QCOMPARE(spy.count(),2);

    delete timer;
}

void tst_qdeclarativetimer::triggeredOnStartRepeat()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(QByteArray("import QtQuick 1.0\nTimer { interval: 100; running: true; triggeredOnStart: true; repeat: true }"), QUrl::fromLocalFile(""));
    QDeclarativeTimer *timer = qobject_cast<QDeclarativeTimer*>(component.create());
    QVERIFY(timer != 0);

    TimerHelper helper;
    connect(timer, SIGNAL(triggered()), &helper, SLOT(timeout()));
    consistentWait(1);
    QCOMPARE(helper.count, 1);

    consistentWait(200);
    QVERIFY(helper.count > 1);
    int oldCount = helper.count;
    consistentWait(200);
    QVERIFY(helper.count > oldCount);
    QVERIFY(timer->isRunning());

    delete timer;
}

void tst_qdeclarativetimer::noTriggerIfNotRunning()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(QByteArray(
        "import QtQuick 1.0\n"
        "Item { property bool ok: true\n"
            "Timer { id: t1; interval: 100; repeat: true; running: true; onTriggered: if (!running) ok=false }"
            "Timer { interval: 10; running: true; onTriggered: t1.running=false }"
        "}"
    ), QUrl::fromLocalFile(""));
    QObject *item = component.create();
    QVERIFY(item != 0);
    consistentWait(200);
    QCOMPARE(item->property("ok").toBool(), true);

    delete item;
}

void tst_qdeclarativetimer::changeDuration()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(QByteArray("import QtQuick 1.0\nTimer { interval: 200; repeat: true; running: true }"), QUrl::fromLocalFile(""));
    QDeclarativeTimer *timer = qobject_cast<QDeclarativeTimer*>(component.create());
    QVERIFY(timer != 0);

    TimerHelper helper;
    connect(timer, SIGNAL(triggered()), &helper, SLOT(timeout()));
    QCOMPARE(helper.count, 0);

    consistentWait(500);
    QCOMPARE(helper.count, 2);

    timer->setInterval(500);

    consistentWait(600);
    QCOMPARE(helper.count, 3);
    QVERIFY(timer->isRunning());

    QSignalSpy spy(timer, SIGNAL(intervalChanged()));

    timer->setInterval(200);
    QCOMPARE(timer->interval(), 200);
    QCOMPARE(spy.count(),1);

    timer->setInterval(200);
    QCOMPARE(spy.count(),1);

    timer->setInterval(300);
    QCOMPARE(spy.count(),2);

    delete timer;
}

void tst_qdeclarativetimer::restart()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(QByteArray("import QtQuick 1.0\nTimer { interval: 500; repeat: true; running: true }"), QUrl::fromLocalFile(""));
    QDeclarativeTimer *timer = qobject_cast<QDeclarativeTimer*>(component.create());
    QVERIFY(timer != 0);

    TimerHelper helper;
    connect(timer, SIGNAL(triggered()), &helper, SLOT(timeout()));
    QCOMPARE(helper.count, 0);

    consistentWait(600);
    QCOMPARE(helper.count, 1);

    consistentWait(300);

    timer->restart();

    consistentWait(700);

    QCOMPARE(helper.count, 2);
    QVERIFY(timer->isRunning());

    delete timer;
}

void tst_qdeclarativetimer::parentProperty()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(QByteArray("import QtQuick 1.0\nItem { Timer { objectName: \"timer\"; running: parent.visible } }"), QUrl::fromLocalFile(""));
    QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item != 0);
    QDeclarativeTimer *timer = item->findChild<QDeclarativeTimer*>("timer");
    QVERIFY(timer != 0);

    QVERIFY(timer->isRunning());

    delete timer;
}

QTEST_MAIN(tst_qdeclarativetimer)

#include "tst_qdeclarativetimer.moc"
