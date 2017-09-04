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
#include <QtTest/QSignalSpy>
#include <qtest.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/private/qqmltimer_p.h>
#include <QtQuick/qquickitem.h>
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

void eventLoopWait(int ms)
{
    // QTest::qWait() always calls sendPostedEvents before exiting, so we can't use it to stop
    // between an event is posted and it is received; But we can use an event loop instead

    QPauseAnimation waitTimer(ms);
    waitTimer.start();
    while (waitTimer.state() == QAbstractAnimation::Running)
    {
        QTimer timer;
        QEventLoop eventLoop;
        timer.start(0);
        timer.connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
        eventLoop.exec();
    }
}

class tst_qqmltimer : public QObject
{
    Q_OBJECT
public:
    tst_qqmltimer();

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
    void restartFromTriggered();
    void runningFromTriggered();
    void parentProperty();
    void stopWhenEventPosted();
    void restartWhenEventPosted();
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

tst_qqmltimer::tst_qqmltimer()
{
}

void tst_qqmltimer::initTestCase()
{
    QUnifiedTimer::instance()->setConsistentTiming(true);
}

void tst_qqmltimer::notRepeating()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQml 2.0\nTimer { interval: 100; running: true }"), QUrl::fromLocalFile(""));
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(component.create());
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
    QVERIFY(!timer->isRunning());
}

void tst_qqmltimer::notRepeatingStart()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQml 2.0\nTimer { interval: 100 }"), QUrl::fromLocalFile(""));
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(component.create());
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
    QVERIFY(!timer->isRunning());

    delete timer;
}

void tst_qqmltimer::repeat()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQml 2.0\nTimer { interval: 100; repeat: true; running: true }"), QUrl::fromLocalFile(""));
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(component.create());
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
    QCOMPARE(helper.count, oldCount);
    QVERIFY(!timer->isRunning());

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

void tst_qqmltimer::triggeredOnStart()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQml 2.0\nTimer { interval: 100; running: true; triggeredOnStart: true }"), QUrl::fromLocalFile(""));
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(component.create());
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
    QVERIFY(!timer->isRunning());

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

void tst_qqmltimer::triggeredOnStartRepeat()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQml 2.0\nTimer { interval: 100; running: true; triggeredOnStart: true; repeat: true }"), QUrl::fromLocalFile(""));
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(component.create());
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

void tst_qqmltimer::noTriggerIfNotRunning()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray(
        "import QtQml 2.0\n"
        "QtObject { property bool ok: true\n"
            "property Timer timer1: Timer { id: t1; interval: 100; repeat: true; running: true; onTriggered: if (!running) ok=false }"
            "property Timer timer2: Timer { interval: 10; running: true; onTriggered: t1.running=false }"
        "}"
    ), QUrl::fromLocalFile(""));
    QObject *item = component.create();
    QVERIFY(item != 0);
    consistentWait(200);
    QCOMPARE(item->property("ok").toBool(), true);

    delete item;
}

void tst_qqmltimer::changeDuration()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQml 2.0\nTimer { interval: 200; repeat: true; running: true }"), QUrl::fromLocalFile(""));
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(component.create());
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

void tst_qqmltimer::restart()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQml 2.0\nTimer { interval: 500; repeat: true; running: true }"), QUrl::fromLocalFile(""));
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(component.create());
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

void tst_qqmltimer::restartFromTriggered()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQml 2.0\nTimer { "
                                    "interval: 500; "
                                    "repeat: false; "
                                    "running: true; "
                                    "onTriggered: restart()"
                                 " }"), QUrl::fromLocalFile(""));
    QScopedPointer<QObject> object(component.create());
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(object.data());
    QVERIFY(timer != 0);

    TimerHelper helper;
    connect(timer, SIGNAL(triggered()), &helper, SLOT(timeout()));
    QCOMPARE(helper.count, 0);

    consistentWait(600);
    QCOMPARE(helper.count, 1);
    QVERIFY(timer->isRunning());

    consistentWait(600);
    QCOMPARE(helper.count, 2);
    QVERIFY(timer->isRunning());
}

void tst_qqmltimer::runningFromTriggered()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQml 2.0\nTimer { "
                                    "property bool ok: false; "
                                    "interval: 500; "
                                    "repeat: false; "
                                    "running: true; "
                                    "onTriggered: { ok = !running; running = true }"
                                 " }"), QUrl::fromLocalFile(""));
    QScopedPointer<QObject> object(component.create());
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(object.data());
    QVERIFY(timer != 0);

    TimerHelper helper;
    connect(timer, SIGNAL(triggered()), &helper, SLOT(timeout()));
    QCOMPARE(helper.count, 0);

    consistentWait(600);
    QCOMPARE(helper.count, 1);
    QVERIFY(timer->property("ok").toBool());
    QVERIFY(timer->isRunning());

    consistentWait(600);
    QCOMPARE(helper.count, 2);
    QVERIFY(timer->property("ok").toBool());
    QVERIFY(timer->isRunning());
}

void tst_qqmltimer::parentProperty()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQuick 2.0\nItem { Timer { objectName: \"timer\"; running: parent.visible } }"), QUrl::fromLocalFile(""));
    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item != 0);
    QQmlTimer *timer = item->findChild<QQmlTimer*>("timer");
    QVERIFY(timer != 0);

    QVERIFY(timer->isRunning());

    delete timer;
}

void tst_qqmltimer::stopWhenEventPosted()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQml 2.0\nTimer { interval: 200; running: true }"), QUrl::fromLocalFile(""));
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(component.create());

    TimerHelper helper;
    connect(timer, SIGNAL(triggered()), &helper, SLOT(timeout()));
    QCOMPARE(helper.count, 0);

    eventLoopWait(200);
    QCOMPARE(helper.count, 0);
    QVERIFY(timer->isRunning());
    timer->stop();
    QVERIFY(!timer->isRunning());

    consistentWait(300);
    QCOMPARE(helper.count, 0);
}


void tst_qqmltimer::restartWhenEventPosted()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQml 2.0\nTimer { interval: 200; running: true }"), QUrl::fromLocalFile(""));
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(component.create());

    TimerHelper helper;
    connect(timer, SIGNAL(triggered()), &helper, SLOT(timeout()));
    QCOMPARE(helper.count, 0);

    eventLoopWait(200);
    QCOMPARE(helper.count, 0);
    timer->restart();

    consistentWait(100);
    QCOMPARE(helper.count, 0);
    QVERIFY(timer->isRunning());

    consistentWait(200);
    QCOMPARE(helper.count, 1);
}

QTEST_MAIN(tst_qqmltimer)

#include "tst_qqmltimer.moc"
