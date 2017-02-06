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

#include <QtQml/private/qpauseanimationjob_p.h>
#include <QtQml/private/qsequentialanimationgroupjob_p.h>
#include <QtQml/private/qparallelanimationgroupjob_p.h>

#ifdef Q_OS_WIN
static const char winTimerError[] = "On windows, consistent timing is not working properly due to bad timer resolution";
#endif

class TestablePauseAnimation : public QPauseAnimationJob
{
public:
    TestablePauseAnimation()
        : m_updateCurrentTimeCount(0)
    {
    }

    TestablePauseAnimation(int duration)
        : QPauseAnimationJob(duration), m_updateCurrentTimeCount(0)
    {
    }

    int m_updateCurrentTimeCount;
protected:
    void updateCurrentTime(int currentTime)
    {
        QPauseAnimationJob::updateCurrentTime(currentTime);
        ++m_updateCurrentTimeCount;
    }
};

class TestableGenericAnimation : public QAbstractAnimationJob
{
public:
    TestableGenericAnimation(int duration = 250) : m_duration(duration) {}
    int duration() const { return m_duration; }

private:
    int m_duration;
};

class EnableConsistentTiming
{
public:
    EnableConsistentTiming()
    {
        QUnifiedTimer *timer = QUnifiedTimer::instance();
        timer->setConsistentTiming(true);
    }
    ~EnableConsistentTiming()
    {
        QUnifiedTimer *timer = QUnifiedTimer::instance();
        timer->setConsistentTiming(false);
    }
};

class tst_QPauseAnimationJob : public QObject
{
  Q_OBJECT
public Q_SLOTS:
    void initTestCase();

private slots:
    void changeDirectionWhileRunning();
    void noTimerUpdates_data();
    void noTimerUpdates();
    void multiplePauseAnimations();
    void pauseAndPropertyAnimations();
    void pauseResume();
    void sequentialPauseGroup();
    void sequentialGroupWithPause();
    void multipleSequentialGroups();
    void zeroDuration();
};

void tst_QPauseAnimationJob::initTestCase()
{
//    qRegisterMetaType<QAbstractAnimationJob::State>("QAbstractAnimationJob::State");
}

void tst_QPauseAnimationJob::changeDirectionWhileRunning()
{
    EnableConsistentTiming enabled;

    TestablePauseAnimation animation;
    animation.setDuration(400);
    animation.start();
    QTest::qWait(100);
    QCOMPARE(animation.state(), QAbstractAnimationJob::Running);
    animation.setDirection(QAbstractAnimationJob::Backward);
    QTest::qWait(animation.totalDuration() + 50);
    QCOMPARE(animation.state(), QAbstractAnimationJob::Stopped);
}

void tst_QPauseAnimationJob::noTimerUpdates_data()
{
    QTest::addColumn<int>("duration");
    QTest::addColumn<int>("loopCount");

    QTest::newRow("0") << 200 << 1;
    QTest::newRow("1") << 160 << 1;
    QTest::newRow("2") << 160 << 2;
    QTest::newRow("3") << 200 << 3;
}

void tst_QPauseAnimationJob::noTimerUpdates()
{
    EnableConsistentTiming enabled;

    QFETCH(int, duration);
    QFETCH(int, loopCount);

    TestablePauseAnimation animation;
    animation.setDuration(duration);
    animation.setLoopCount(loopCount);
    animation.start();
    QTest::qWait(animation.totalDuration() + 100);

#ifdef Q_OS_WIN
    if (animation.state() != QAbstractAnimationJob::Stopped)
        QEXPECT_FAIL("", winTimerError, Abort);
#endif

    QCOMPARE(animation.state(), QAbstractAnimationJob::Stopped);
    const int expectedLoopCount = 1 + loopCount;

#ifdef Q_OS_WIN
    if (animation.m_updateCurrentTimeCount != expectedLoopCount)
        QEXPECT_FAIL("", winTimerError, Abort);
#endif
    QCOMPARE(animation.m_updateCurrentTimeCount, expectedLoopCount);
}

void tst_QPauseAnimationJob::multiplePauseAnimations()
{
    EnableConsistentTiming enabled;

    TestablePauseAnimation animation;
    animation.setDuration(200);

    TestablePauseAnimation animation2;
    animation2.setDuration(800);

    animation.start();
    animation2.start();
    QTest::qWait(animation.totalDuration() + 100);

#ifdef Q_OS_WIN
    if (animation.state() != QAbstractAnimationJob::Stopped)
        QEXPECT_FAIL("", winTimerError, Abort);
#endif
    QCOMPARE(animation.state(), QAbstractAnimationJob::Stopped);

#ifdef Q_OS_WIN
    if (animation2.state() != QAbstractAnimationJob::Running)
        QEXPECT_FAIL("", winTimerError, Abort);
#endif
    QCOMPARE(animation2.state(), QAbstractAnimationJob::Running);

#ifdef Q_OS_WIN
    if (animation.m_updateCurrentTimeCount != 2)
        QEXPECT_FAIL("", winTimerError, Abort);
#endif
    QCOMPARE(animation.m_updateCurrentTimeCount, 2);

#ifdef Q_OS_WIN
    if (animation2.m_updateCurrentTimeCount != 2)
        QEXPECT_FAIL("", winTimerError, Abort);
#endif
    QCOMPARE(animation2.m_updateCurrentTimeCount, 2);

    QTRY_COMPARE(animation2.state(), QAbstractAnimationJob::Stopped);
    QVERIFY(animation2.m_updateCurrentTimeCount >= 3);
}

void tst_QPauseAnimationJob::pauseAndPropertyAnimations()
{
    EnableConsistentTiming enabled;

    TestablePauseAnimation pause;
    pause.setDuration(200);

    TestableGenericAnimation animation;

    pause.start();

    QTest::qWait(100);
    animation.start();

    QCOMPARE(animation.state(), QAbstractAnimationJob::Running);

    QTRY_COMPARE(animation.state(), QAbstractAnimationJob::Running);
    QCOMPARE(pause.state(), QAbstractAnimationJob::Running);
    QVERIFY2(pause.m_updateCurrentTimeCount >= 2,
             QByteArrayLiteral("pause.m_updateCurrentTimeCount=") + QByteArray::number(pause.m_updateCurrentTimeCount));

    QTRY_COMPARE(animation.state(), QAbstractAnimationJob::Stopped);
    QCOMPARE(pause.state(), QAbstractAnimationJob::Stopped);
    QVERIFY2(pause.m_updateCurrentTimeCount > 3,
             QByteArrayLiteral("pause.m_updateCurrentTimeCount=") + QByteArray::number(pause.m_updateCurrentTimeCount));
}

void tst_QPauseAnimationJob::pauseResume()
{
    TestablePauseAnimation animation;
    animation.setDuration(400);
    animation.start();
    QCOMPARE(animation.state(), QAbstractAnimationJob::Running);
    QTest::qWait(200);
    animation.pause();
    QCOMPARE(animation.state(), QAbstractAnimationJob::Paused);
    animation.start();
    QTest::qWait(300);
    QTRY_COMPARE(animation.state(), QAbstractAnimationJob::Stopped);
    QVERIFY2(animation.m_updateCurrentTimeCount >= 3,
            QByteArrayLiteral("animation.m_updateCurrentTimeCount=") + QByteArray::number(animation.m_updateCurrentTimeCount));
}

void tst_QPauseAnimationJob::sequentialPauseGroup()
{
    QSequentialAnimationGroupJob group;

    TestablePauseAnimation animation1(200);
    group.appendAnimation(&animation1);
    TestablePauseAnimation animation2(200);
    group.appendAnimation(&animation2);
    TestablePauseAnimation animation3(200);
    group.appendAnimation(&animation3);

    group.start();
    QCOMPARE(animation1.m_updateCurrentTimeCount, 1);
    QCOMPARE(animation2.m_updateCurrentTimeCount, 0);
    QCOMPARE(animation3.m_updateCurrentTimeCount, 0);

    QCOMPARE(group.state(), QAbstractAnimationJob::Running);
    QCOMPARE(animation1.state(), QAbstractAnimationJob::Running);
    QCOMPARE(animation2.state(), QAbstractAnimationJob::Stopped);
    QCOMPARE(animation3.state(), QAbstractAnimationJob::Stopped);

    group.setCurrentTime(250);
    QCOMPARE(animation1.m_updateCurrentTimeCount, 2);
    QCOMPARE(animation2.m_updateCurrentTimeCount, 1);
    QCOMPARE(animation3.m_updateCurrentTimeCount, 0);

    QCOMPARE(group.state(), QAbstractAnimationJob::Running);
    QCOMPARE(animation1.state(), QAbstractAnimationJob::Stopped);
    QCOMPARE((QAbstractAnimationJob*)&animation2, group.currentAnimation());
    QCOMPARE(animation2.state(), QAbstractAnimationJob::Running);
    QCOMPARE(animation3.state(), QAbstractAnimationJob::Stopped);

    group.setCurrentTime(500);
    QCOMPARE(animation1.m_updateCurrentTimeCount, 2);
    QCOMPARE(animation2.m_updateCurrentTimeCount, 2);
    QCOMPARE(animation3.m_updateCurrentTimeCount, 1);

    QCOMPARE(group.state(), QAbstractAnimationJob::Running);
    QCOMPARE(animation1.state(), QAbstractAnimationJob::Stopped);
    QCOMPARE(animation2.state(), QAbstractAnimationJob::Stopped);
    QCOMPARE((QAbstractAnimationJob*)&animation3, group.currentAnimation());
    QCOMPARE(animation3.state(), QAbstractAnimationJob::Running);

    group.setCurrentTime(750);

    QCOMPARE(group.state(), QAbstractAnimationJob::Stopped);
    QCOMPARE(animation1.state(), QAbstractAnimationJob::Stopped);
    QCOMPARE(animation2.state(), QAbstractAnimationJob::Stopped);
    QCOMPARE(animation3.state(), QAbstractAnimationJob::Stopped);

    QCOMPARE(animation1.m_updateCurrentTimeCount, 2);
    QCOMPARE(animation2.m_updateCurrentTimeCount, 2);
    QCOMPARE(animation3.m_updateCurrentTimeCount, 2);
}

void tst_QPauseAnimationJob::sequentialGroupWithPause()
{
    QSequentialAnimationGroupJob group;

    TestableGenericAnimation animation;
    group.appendAnimation(&animation);

    TestablePauseAnimation pause;
    pause.setDuration(250);
    group.appendAnimation(&pause);

    group.start();

    QCOMPARE(group.state(), QAbstractAnimationJob::Running);
    QCOMPARE(animation.state(), QAbstractAnimationJob::Running);
    QCOMPARE(pause.state(), QAbstractAnimationJob::Stopped);

    group.setCurrentTime(300);

    QCOMPARE(group.state(), QAbstractAnimationJob::Running);
    QCOMPARE(animation.state(), QAbstractAnimationJob::Stopped);
    QCOMPARE((QAbstractAnimationJob*)&pause, group.currentAnimation());
    QCOMPARE(pause.state(), QAbstractAnimationJob::Running);

    group.setCurrentTime(600);

    QCOMPARE(group.state(), QAbstractAnimationJob::Stopped);
    QCOMPARE(animation.state(), QAbstractAnimationJob::Stopped);
    QCOMPARE(pause.state(), QAbstractAnimationJob::Stopped);

    QCOMPARE(pause.m_updateCurrentTimeCount, 2);
}

void tst_QPauseAnimationJob::multipleSequentialGroups()
{
    EnableConsistentTiming enabled;

    QParallelAnimationGroupJob group;
    group.setLoopCount(2);

    QSequentialAnimationGroupJob subgroup1;
    group.appendAnimation(&subgroup1);

    TestableGenericAnimation animation(300);
    subgroup1.appendAnimation(&animation);

    TestablePauseAnimation pause(200);
    subgroup1.appendAnimation(&pause);

    QSequentialAnimationGroupJob subgroup2;
    group.appendAnimation(&subgroup2);

    TestableGenericAnimation animation2(200);
    subgroup2.appendAnimation(&animation2);

    TestablePauseAnimation pause2(250);
    subgroup2.appendAnimation(&pause2);

    QSequentialAnimationGroupJob subgroup3;
    group.appendAnimation(&subgroup3);

    TestablePauseAnimation pause3(400);
    subgroup3.appendAnimation(&pause3);

    TestableGenericAnimation animation3(200);
    subgroup3.appendAnimation(&animation3);

    QSequentialAnimationGroupJob subgroup4;
    group.appendAnimation(&subgroup4);

    TestablePauseAnimation pause4(310);
    subgroup4.appendAnimation(&pause4);

    TestablePauseAnimation pause5(60);
    subgroup4.appendAnimation(&pause5);

    group.start();

    QCOMPARE(group.state(), QAbstractAnimationJob::Running);
    QCOMPARE(subgroup1.state(), QAbstractAnimationJob::Running);
    QCOMPARE(subgroup2.state(), QAbstractAnimationJob::Running);
    QCOMPARE(subgroup3.state(), QAbstractAnimationJob::Running);
    QCOMPARE(subgroup4.state(), QAbstractAnimationJob::Running);

    // This is a pretty long animation so it tends to get rather out of sync
    // when using the consistent timer, so run for an extra half second for good
    // measure...
    QTest::qWait(group.totalDuration() + 500);

#ifdef Q_OS_WIN
    if (group.state() != QAbstractAnimationJob::Stopped)
        QEXPECT_FAIL("", winTimerError, Abort);
#endif
    QTRY_COMPARE(group.state(), QAbstractAnimationJob::Stopped);

#ifdef Q_OS_WIN
    if (subgroup1.state() != QAbstractAnimationJob::Stopped)
        QEXPECT_FAIL("", winTimerError, Abort);
#endif
    QCOMPARE(subgroup1.state(), QAbstractAnimationJob::Stopped);

#ifdef Q_OS_WIN
    if (subgroup2.state() != QAbstractAnimationJob::Stopped)
        QEXPECT_FAIL("", winTimerError, Abort);
#endif
    QCOMPARE(subgroup2.state(), QAbstractAnimationJob::Stopped);

#ifdef Q_OS_WIN
    if (subgroup3.state() != QAbstractAnimationJob::Stopped)
        QEXPECT_FAIL("", winTimerError, Abort);
#endif
    QCOMPARE(subgroup3.state(), QAbstractAnimationJob::Stopped);

#ifdef Q_OS_WIN
    if (subgroup4.state() != QAbstractAnimationJob::Stopped)
        QEXPECT_FAIL("", winTimerError, Abort);
#endif
    QCOMPARE(subgroup4.state(), QAbstractAnimationJob::Stopped);

#ifdef Q_OS_WIN
    if (pause5.m_updateCurrentTimeCount != 4)
        QEXPECT_FAIL("", winTimerError, Abort);
#endif
    QCOMPARE(pause5.m_updateCurrentTimeCount, 4);
}

void tst_QPauseAnimationJob::zeroDuration()
{
    TestablePauseAnimation animation;
    animation.setDuration(0);
    animation.start();
    QTest::qWait(animation.totalDuration() + 100);
    QCOMPARE(animation.state(), QAbstractAnimationJob::Stopped);
    QCOMPARE(animation.m_updateCurrentTimeCount, 1);
}

QTEST_MAIN(tst_QPauseAnimationJob)
#include "tst_qpauseanimationjob.moc"
