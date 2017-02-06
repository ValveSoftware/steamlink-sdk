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

#include <QtQml/private/qparallelanimationgroupjob_p.h>

Q_DECLARE_METATYPE(QAbstractAnimationJob::State)

class tst_QParallelAnimationGroupJob : public QObject
{
    Q_OBJECT
public Q_SLOTS:
    void initTestCase();

private slots:
    void construction();
    void setCurrentTime();
    void stateChanged();
    void clearGroup();
    void propagateGroupUpdateToChildren();
    void updateChildrenWithRunningGroup();
    void deleteChildrenWithRunningGroup();
    void startChildrenWithStoppedGroup();
    void stopGroupWithRunningChild();
    void startGroupWithRunningChild();
    void zeroDurationAnimation();
    void stopUncontrolledAnimations();
    void uncontrolledWithLoops();
    void loopCount_data();
    void loopCount();
    void addAndRemoveDuration();
    void pauseResume();

    void crashWhenRemovingUncontrolledAnimation();
};

void tst_QParallelAnimationGroupJob::initTestCase()
{
    qRegisterMetaType<QAbstractAnimationJob::State>("QAbstractAnimationJob::State");
#if defined(Q_OS_DARWIN)
    // give the Apple application's start event queue time to clear
    QTest::qWait(1000);
#endif
}

void tst_QParallelAnimationGroupJob::construction()
{
    QParallelAnimationGroupJob animationgroup;
}

class TestAnimation : public QAbstractAnimationJob
{
public:
    TestAnimation(int duration = 250) : m_duration(duration) {}
    int duration() const { return m_duration; }

private:
    int m_duration;
};

class UncontrolledAnimation : public QObject, public QAbstractAnimationJob
{
    Q_OBJECT
public:
    UncontrolledAnimation()
        : id(0)
    {
    }

    int duration() const { return -1; /* not time driven */ }

protected:
    void timerEvent(QTimerEvent *event)
    {
        if (event->timerId() == id)
            stop();
    }

    void updateRunning(bool running)
    {
        if (running) {
            id = startTimer(500);
        } else {
            killTimer(id);
            id = 0;
        }
    }

private:
    int id;
};

class StateChangeListener: public QAnimationJobChangeListener
{
public:
    virtual void animationStateChanged(QAbstractAnimationJob *, QAbstractAnimationJob::State newState, QAbstractAnimationJob::State)
    {
        states << newState;
    }

    void clear() { states.clear(); }
    int count() { return states.count(); }

    QList<QAbstractAnimationJob::State> states;
};

class FinishedListener: public QAnimationJobChangeListener
{
public:
    FinishedListener() : m_count(0) {}

    virtual void animationFinished(QAbstractAnimationJob *) { ++m_count; }
    void clear() { m_count = 0; }
    int count() { return m_count; }

private:
    int m_count;
};

void tst_QParallelAnimationGroupJob::setCurrentTime()
{
    // originally was parallel operating on different object/properties
    QAnimationGroupJob *parallel = new QParallelAnimationGroupJob();
    TestAnimation *a1_p_o1 = new TestAnimation;
    TestAnimation *a1_p_o2 = new TestAnimation;
    TestAnimation *a1_p_o3 = new TestAnimation;
    a1_p_o2->setLoopCount(3);
    parallel->appendAnimation(a1_p_o1);
    parallel->appendAnimation(a1_p_o2);
    parallel->appendAnimation(a1_p_o3);

    UncontrolledAnimation *notTimeDriven = new UncontrolledAnimation;
    QCOMPARE(notTimeDriven->totalDuration(), -1);

    TestAnimation *loopsForever = new TestAnimation;
    loopsForever->setLoopCount(-1);
    QCOMPARE(loopsForever->totalDuration(), -1);

    QParallelAnimationGroupJob group;
    group.appendAnimation(parallel);
    group.appendAnimation(notTimeDriven);
    group.appendAnimation(loopsForever);

    // Current time = 1
    group.setCurrentTime(1);
    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(parallel->state(), QAnimationGroupJob::Stopped);
    QCOMPARE(a1_p_o1->state(), QAnimationGroupJob::Stopped);
    QCOMPARE(a1_p_o2->state(), QAnimationGroupJob::Stopped);
    QCOMPARE(a1_p_o3->state(), QAnimationGroupJob::Stopped);
    QCOMPARE(notTimeDriven->state(), QAnimationGroupJob::Stopped);
    QCOMPARE(loopsForever->state(), QAnimationGroupJob::Stopped);

    QCOMPARE(group.currentLoopTime(), 1);
    QCOMPARE(a1_p_o1->currentLoopTime(), 1);
    QCOMPARE(a1_p_o2->currentLoopTime(), 1);
    QCOMPARE(a1_p_o3->currentLoopTime(), 1);
    QCOMPARE(notTimeDriven->currentLoopTime(), 1);
    QCOMPARE(loopsForever->currentLoopTime(), 1);

    // Current time = 250
    group.setCurrentTime(250);
    QCOMPARE(group.currentLoopTime(), 250);
    QCOMPARE(a1_p_o1->currentLoopTime(), 250);
    QCOMPARE(a1_p_o2->currentLoopTime(), 0);
    QCOMPARE(a1_p_o2->currentLoop(), 1);
    QCOMPARE(a1_p_o3->currentLoopTime(), 250);
    QCOMPARE(notTimeDriven->currentLoopTime(), 250);
    QCOMPARE(loopsForever->currentLoopTime(), 0);
    QCOMPARE(loopsForever->currentLoop(), 1);

    // Current time = 251
    group.setCurrentTime(251);
    QCOMPARE(group.currentLoopTime(), 251);
    QCOMPARE(a1_p_o1->currentLoopTime(), 250);
    QCOMPARE(a1_p_o2->currentLoopTime(), 1);
    QCOMPARE(a1_p_o2->currentLoop(), 1);
    QCOMPARE(a1_p_o3->currentLoopTime(), 250);
    QCOMPARE(notTimeDriven->currentLoopTime(), 251);
    QCOMPARE(loopsForever->currentLoopTime(), 1);
}

void tst_QParallelAnimationGroupJob::stateChanged()
{
    //this ensures that the correct animations are started when starting the group
    TestAnimation *anim1 = new TestAnimation(1000);
    TestAnimation *anim2 = new TestAnimation(2000);
    TestAnimation *anim3 = new TestAnimation(3000);
    TestAnimation *anim4 = new TestAnimation(3000);

    QParallelAnimationGroupJob group;
    group.appendAnimation(anim1);
    group.appendAnimation(anim2);
    group.appendAnimation(anim3);
    group.appendAnimation(anim4);

    StateChangeListener spy1;
    anim1->addAnimationChangeListener(&spy1, QAbstractAnimationJob::StateChange);
    StateChangeListener spy2;
    anim2->addAnimationChangeListener(&spy2, QAbstractAnimationJob::StateChange);
    StateChangeListener spy3;
    anim3->addAnimationChangeListener(&spy3, QAbstractAnimationJob::StateChange);
    StateChangeListener spy4;
    anim4->addAnimationChangeListener(&spy4, QAbstractAnimationJob::StateChange);

    //first; let's start forward
    group.start();
    //all the animations should be started
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy1.states.last(), TestAnimation::Running);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy2.states.last(), TestAnimation::Running);
    QCOMPARE(spy3.count(), 1);
    QCOMPARE(spy3.states.last(), TestAnimation::Running);
    QCOMPARE(spy4.count(), 1);
    QCOMPARE(spy4.states.last(), TestAnimation::Running);

    group.setCurrentTime(1500); //anim1 should be finished
    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(spy1.count(), 2);
    QCOMPARE(spy1.states.last(), TestAnimation::Stopped);
    QCOMPARE(spy2.count(), 1); //no change
    QCOMPARE(spy3.count(), 1); //no change
    QCOMPARE(spy4.count(), 1); //no change

    group.setCurrentTime(2500); //anim2 should be finished
    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(spy1.count(), 2); //no change
    QCOMPARE(spy2.count(), 2);
    QCOMPARE(spy2.states.last(), TestAnimation::Stopped);
    QCOMPARE(spy3.count(), 1); //no change
    QCOMPARE(spy4.count(), 1); //no change

    group.setCurrentTime(3500); //everything should be finished
    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(spy1.count(), 2); //no change
    QCOMPARE(spy2.count(), 2); //no change
    QCOMPARE(spy3.count(), 2);
    QCOMPARE(spy3.states.last(), TestAnimation::Stopped);
    QCOMPARE(spy4.count(), 2);
    QCOMPARE(spy4.states.last(), TestAnimation::Stopped);

    //cleanup
    spy1.clear();
    spy2.clear();
    spy3.clear();
    spy4.clear();

    //now let's try to reverse that
    group.setDirection(QAbstractAnimationJob::Backward);
    group.start();

    //only anim3 and anim4 should be started
    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 1);
    QCOMPARE(spy3.states.last(), TestAnimation::Running);
    QCOMPARE(spy4.count(), 1);
    QCOMPARE(spy4.states.last(), TestAnimation::Running);

    group.setCurrentTime(1500); //anim2 should be started
    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(spy1.count(), 0); //no change
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy2.states.last(), TestAnimation::Running);
    QCOMPARE(spy3.count(), 1); //no change
    QCOMPARE(spy4.count(), 1); //no change

    group.setCurrentTime(500); //anim1 is finally also started
    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy1.states.last(), TestAnimation::Running);
    QCOMPARE(spy2.count(), 1); //no change
    QCOMPARE(spy3.count(), 1); //no change
    QCOMPARE(spy4.count(), 1); //no change

    group.setCurrentTime(0); //everything should be stopped
    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(spy1.count(), 2);
    QCOMPARE(spy1.states.last(), TestAnimation::Stopped);
    QCOMPARE(spy2.count(), 2);
    QCOMPARE(spy2.states.last(), TestAnimation::Stopped);
    QCOMPARE(spy3.count(), 2);
    QCOMPARE(spy3.states.last(), TestAnimation::Stopped);
    QCOMPARE(spy4.count(), 2);
    QCOMPARE(spy4.states.last(), TestAnimation::Stopped);
}

void tst_QParallelAnimationGroupJob::clearGroup()
{
    QParallelAnimationGroupJob group;
    static const int animationCount = 10;

    for (int i = 0; i < animationCount; ++i) {
        group.appendAnimation(new QParallelAnimationGroupJob);
    }

    int count = 0;
    for (QAbstractAnimationJob *anim = group.firstChild(); anim; anim = anim->nextSibling())
        ++count;
    QCOMPARE(count, animationCount);

    group.clear();

    QVERIFY(!group.firstChild() && !group.lastChild());
    QCOMPARE(group.currentLoopTime(), 0);
}

void tst_QParallelAnimationGroupJob::propagateGroupUpdateToChildren()
{
    // this test verifies if group state changes are updating its children correctly
    QParallelAnimationGroupJob group;

    TestAnimation anim1(100);
    TestAnimation anim2(200);

    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Stopped);

    group.appendAnimation(&anim1);
    group.appendAnimation(&anim2);

    group.start();

    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Running);

    group.pause();

    QCOMPARE(group.state(), QAnimationGroupJob::Paused);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Paused);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Paused);

    group.stop();

    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Stopped);
}

void tst_QParallelAnimationGroupJob::updateChildrenWithRunningGroup()
{
    // assert that its possible to modify a child's state directly while their group is running
    QParallelAnimationGroupJob group;

    TestAnimation anim(200);

    StateChangeListener groupStateChangedSpy;
    group.addAnimationChangeListener(&groupStateChangedSpy, QAbstractAnimationJob::StateChange);
    StateChangeListener childStateChangedSpy;
    anim.addAnimationChangeListener(&childStateChangedSpy, QAbstractAnimationJob::StateChange);

    QCOMPARE(groupStateChangedSpy.count(), 0);
    QCOMPARE(childStateChangedSpy.count(), 0);
    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim.state(), QAnimationGroupJob::Stopped);

    group.appendAnimation(&anim);

    group.start();

    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim.state(), QAnimationGroupJob::Running);

    QCOMPARE(groupStateChangedSpy.count(), 1);
    QCOMPARE(childStateChangedSpy.count(), 1);

    QCOMPARE(groupStateChangedSpy.states.at(0), QAnimationGroupJob::Running);
    QCOMPARE(childStateChangedSpy.states.at(0), QAnimationGroupJob::Running);

    // starting directly a running child will not have any effect
    anim.start();

    QCOMPARE(groupStateChangedSpy.count(), 1);
    QCOMPARE(childStateChangedSpy.count(), 1);

    anim.pause();

    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim.state(), QAnimationGroupJob::Paused);

    // in the animation stops directly, the group will still be running
    anim.stop();

    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim.state(), QAnimationGroupJob::Stopped);

    //cleanup
    group.removeAnimationChangeListener(&groupStateChangedSpy, QAbstractAnimationJob::StateChange);
    anim.removeAnimationChangeListener(&childStateChangedSpy, QAbstractAnimationJob::StateChange);
}

void tst_QParallelAnimationGroupJob::deleteChildrenWithRunningGroup()
{
    // test if children can be activated when their group is stopped
    QParallelAnimationGroupJob group;

    TestAnimation *anim1 = new TestAnimation(200);
    group.appendAnimation(anim1);

    QCOMPARE(group.duration(), anim1->duration());

    group.start();
    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim1->state(), QAnimationGroupJob::Running);

    QTest::qWait(80);
    QVERIFY(group.currentLoopTime() > 0);

    delete anim1;
    QVERIFY(!group.firstChild());
    QCOMPARE(group.duration(), 0);
    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(group.currentLoopTime(), 0); //that's the invariant
}

void tst_QParallelAnimationGroupJob::startChildrenWithStoppedGroup()
{
    // test if children can be activated when their group is stopped
    QParallelAnimationGroupJob group;

    TestAnimation anim1(200);
    TestAnimation anim2(200);

    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Stopped);

    group.appendAnimation(&anim1);
    group.appendAnimation(&anim2);

    group.stop();

    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Stopped);

    anim1.start();
    anim2.start();
    anim2.pause();

    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Paused);
}

void tst_QParallelAnimationGroupJob::stopGroupWithRunningChild()
{
    // children that started independently will not be affected by a group stop
    QParallelAnimationGroupJob group;

    TestAnimation anim1(200);
    TestAnimation anim2(200);

    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Stopped);

    group.appendAnimation(&anim1);
    group.appendAnimation(&anim2);

    anim1.start();
    anim2.start();
    anim2.pause();

    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Paused);

    group.stop();

    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Paused);

    anim1.stop();
    anim2.stop();

    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Stopped);
}

void tst_QParallelAnimationGroupJob::startGroupWithRunningChild()
{
    // as the group has precedence over its children, starting a group will restart all the children
    QParallelAnimationGroupJob group;

    TestAnimation anim1(200);
    TestAnimation anim2(200);

    StateChangeListener stateChangedSpy1;
    anim1.addAnimationChangeListener(&stateChangedSpy1, QAbstractAnimationJob::StateChange);
    StateChangeListener stateChangedSpy2;
    anim2.addAnimationChangeListener(&stateChangedSpy2, QAbstractAnimationJob::StateChange);

    QCOMPARE(stateChangedSpy1.count(), 0);
    QCOMPARE(stateChangedSpy2.count(), 0);
    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Stopped);

    group.appendAnimation(&anim1);
    group.appendAnimation(&anim2);

    anim1.start();
    anim2.start();
    anim2.pause();

    QCOMPARE(stateChangedSpy1.states.at(0), QAnimationGroupJob::Running);
    QCOMPARE(stateChangedSpy2.states.at(0), QAnimationGroupJob::Running);
    QCOMPARE(stateChangedSpy2.states.at(1), QAnimationGroupJob::Paused);

    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Paused);

    group.start();

    QCOMPARE(stateChangedSpy1.count(), 3);
    QCOMPARE(stateChangedSpy1.states.at(1), QAnimationGroupJob::Stopped);
    QCOMPARE(stateChangedSpy1.states.at(2), QAnimationGroupJob::Running);

    QCOMPARE(stateChangedSpy2.count(), 4);
    QCOMPARE(stateChangedSpy2.states.at(2), QAnimationGroupJob::Stopped);
    QCOMPARE(stateChangedSpy2.states.at(3), QAnimationGroupJob::Running);

    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Running);

    //cleanup
    anim1.removeAnimationChangeListener(&stateChangedSpy1, QAbstractAnimationJob::StateChange);
    anim2.removeAnimationChangeListener(&stateChangedSpy2, QAbstractAnimationJob::StateChange);
}

void tst_QParallelAnimationGroupJob::zeroDurationAnimation()
{
    QParallelAnimationGroupJob group;

    TestAnimation anim1(0);
    TestAnimation anim2(100);
    TestAnimation anim3(10);

    StateChangeListener stateChangedSpy1;
    anim1.addAnimationChangeListener(&stateChangedSpy1, QAbstractAnimationJob::StateChange);
    FinishedListener finishedSpy1;
    anim1.addAnimationChangeListener(&finishedSpy1, QAbstractAnimationJob::Completion);

    StateChangeListener stateChangedSpy2;
    anim2.addAnimationChangeListener(&stateChangedSpy2, QAbstractAnimationJob::StateChange);
    FinishedListener finishedSpy2;
    anim2.addAnimationChangeListener(&finishedSpy2, QAbstractAnimationJob::Completion);

    StateChangeListener stateChangedSpy3;
    anim3.addAnimationChangeListener(&stateChangedSpy3, QAbstractAnimationJob::StateChange);
    FinishedListener finishedSpy3;
    anim3.addAnimationChangeListener(&finishedSpy3, QAbstractAnimationJob::Completion);

    group.appendAnimation(&anim1);
    group.appendAnimation(&anim2);
    group.appendAnimation(&anim3);
    QCOMPARE(stateChangedSpy1.count(), 0);
    group.start();
    QCOMPARE(stateChangedSpy1.count(), 2);
    QCOMPARE(finishedSpy1.count(), 1);
    QCOMPARE(stateChangedSpy1.states.at(0), QAnimationGroupJob::Running);
    QCOMPARE(stateChangedSpy1.states.at(1), QAnimationGroupJob::Stopped);

    QCOMPARE(stateChangedSpy2.count(), 1);
    QCOMPARE(finishedSpy2.count(), 0);
    QCOMPARE(stateChangedSpy1.states.at(0), QAnimationGroupJob::Running);

    QCOMPARE(stateChangedSpy3.count(), 1);
    QCOMPARE(finishedSpy3.count(), 0);
    QCOMPARE(stateChangedSpy3.states.at(0), QAnimationGroupJob::Running);

    QCOMPARE(anim1.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(anim2.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim3.state(), QAnimationGroupJob::Running);
    QCOMPARE(group.state(), QAnimationGroupJob::Running);

    group.stop();
    group.setLoopCount(4);
    stateChangedSpy1.clear();
    stateChangedSpy2.clear();
    stateChangedSpy3.clear();

    group.start();
    QCOMPARE(stateChangedSpy1.count(), 2);
    QCOMPARE(stateChangedSpy2.count(), 1);
    QCOMPARE(stateChangedSpy3.count(), 1);
    group.setCurrentTime(50);
    QCOMPARE(stateChangedSpy1.count(), 2);
    QCOMPARE(stateChangedSpy2.count(), 1);
    QCOMPARE(stateChangedSpy3.count(), 2);
    group.setCurrentTime(150);
    QCOMPARE(stateChangedSpy1.count(), 4);
    QCOMPARE(stateChangedSpy2.count(), 3);
    QCOMPARE(stateChangedSpy3.count(), 4);
    group.setCurrentTime(50);
    QCOMPARE(stateChangedSpy1.count(), 6);
    QCOMPARE(stateChangedSpy2.count(), 5);
    QCOMPARE(stateChangedSpy3.count(), 6);

    //cleanup
    anim1.removeAnimationChangeListener(&stateChangedSpy1, QAbstractAnimationJob::StateChange);
    anim1.removeAnimationChangeListener(&finishedSpy1, QAbstractAnimationJob::Completion);
    anim2.removeAnimationChangeListener(&stateChangedSpy2, QAbstractAnimationJob::StateChange);
    anim2.removeAnimationChangeListener(&finishedSpy2, QAbstractAnimationJob::Completion);
    anim3.removeAnimationChangeListener(&stateChangedSpy3, QAbstractAnimationJob::StateChange);
    anim3.removeAnimationChangeListener(&finishedSpy3, QAbstractAnimationJob::Completion);
}

void tst_QParallelAnimationGroupJob::stopUncontrolledAnimations()
{
    QParallelAnimationGroupJob group;

    TestAnimation anim1(0);

    UncontrolledAnimation notTimeDriven;
    QCOMPARE(notTimeDriven.totalDuration(), -1);

    TestAnimation loopsForever(100);
    loopsForever.setLoopCount(-1);

    StateChangeListener stateChangedSpy;
    anim1.addAnimationChangeListener(&stateChangedSpy, QAbstractAnimationJob::StateChange);

    group.appendAnimation(&anim1);
    group.appendAnimation(&notTimeDriven);
    group.appendAnimation(&loopsForever);

    group.start();

    QCOMPARE(stateChangedSpy.count(), 2);
    QCOMPARE(stateChangedSpy.states.at(0), QAnimationGroupJob::Running);
    QCOMPARE(stateChangedSpy.states.at(1), QAnimationGroupJob::Stopped);

    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(notTimeDriven.state(), QAnimationGroupJob::Running);
    QCOMPARE(loopsForever.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim1.state(), QAnimationGroupJob::Stopped);

    notTimeDriven.stop();

    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(notTimeDriven.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(loopsForever.state(), QAnimationGroupJob::Running);

    loopsForever.stop();

    QCOMPARE(group.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(notTimeDriven.state(), QAnimationGroupJob::Stopped);
    QCOMPARE(loopsForever.state(), QAnimationGroupJob::Stopped);
}

struct AnimState {
    AnimState(int time = -1) : time(time), state(-1) {}
    AnimState(int time, int state) : time(time), state(state) {}
    int time;
    int state;
};

#define Running QAbstractAnimationJob::Running
#define Stopped QAbstractAnimationJob::Stopped

Q_DECLARE_METATYPE(AnimState)
void tst_QParallelAnimationGroupJob::loopCount_data()
{
    QTest::addColumn<bool>("directionBackward");
    QTest::addColumn<int>("setLoopCount");
    QTest::addColumn<int>("initialGroupTime");
    QTest::addColumn<int>("currentGroupTime");
    QTest::addColumn<AnimState>("expected1");
    QTest::addColumn<AnimState>("expected2");
    QTest::addColumn<AnimState>("expected3");

    //                                                                                  D U R A T I O N
    //                                                              100                           60*2                           0
    // direction = Forward
    QTest::newRow("50")  << false << 3 << 0 <<  50 << AnimState( 50, Running) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("100") << false << 3 << 0 << 100 << AnimState(100         ) << AnimState( 40, Running) << AnimState(  0, Stopped);
    QTest::newRow("110") << false << 3 << 0 << 110 << AnimState(100, Stopped) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("120") << false << 3 << 0 << 120 << AnimState(  0, Running) << AnimState(  0, Running) << AnimState(  0, Stopped);

    QTest::newRow("170") << false << 3 << 0 << 170 << AnimState( 50, Running) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("220") << false << 3 << 0 << 220 << AnimState(100         ) << AnimState( 40, Running) << AnimState(  0, Stopped);
    QTest::newRow("230") << false << 3 << 0 << 230 << AnimState(100, Stopped) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("240") << false << 3 << 0 << 240 << AnimState(  0, Running) << AnimState(  0, Running) << AnimState(  0, Stopped);

    QTest::newRow("290") << false << 3 << 0 << 290 << AnimState( 50, Running) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("340") << false << 3 << 0 << 340 << AnimState(100         ) << AnimState( 40, Running) << AnimState(  0, Stopped);
    QTest::newRow("350") << false << 3 << 0 << 350 << AnimState(100, Stopped) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("360") << false << 3 << 0 << 360 << AnimState(100, Stopped) << AnimState( 60         ) << AnimState(  0, Stopped);

    QTest::newRow("410") << false << 3 << 0 << 410 << AnimState(100, Stopped) << AnimState( 60, Stopped) << AnimState(  0, Stopped);
    QTest::newRow("460") << false << 3 << 0 << 460 << AnimState(100, Stopped) << AnimState( 60, Stopped) << AnimState(  0, Stopped);
    QTest::newRow("470") << false << 3 << 0 << 470 << AnimState(100, Stopped) << AnimState( 60, Stopped) << AnimState(  0, Stopped);
    QTest::newRow("480") << false << 3 << 0 << 480 << AnimState(100, Stopped) << AnimState( 60, Stopped) << AnimState(  0, Stopped);

    // direction = Forward, rewind
    QTest::newRow("120-110") << false << 3 << 120 << 110 << AnimState(   0, Stopped) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("120-50")  << false << 3 << 120 <<  50 << AnimState(  50, Running) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("120-0")   << false << 3 << 120 <<  0  << AnimState(   0, Running) << AnimState(  0, Running) << AnimState(  0, Stopped);
    QTest::newRow("300-110") << false << 3 << 300 << 110 << AnimState(   0, Stopped) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("300-50")  << false << 3 << 300 <<  50 << AnimState(  50, Running) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("300-0")   << false << 3 << 300 <<  0  << AnimState(   0, Running) << AnimState(  0, Running) << AnimState(  0, Stopped);
    QTest::newRow("115-105") << false << 3 << 115 << 105 << AnimState(  42, Stopped) << AnimState( 45, Running) << AnimState(  0, Stopped);

    // direction = Backward
    QTest::newRow("b120-120") << true << 3 << 120 << 120 << AnimState( 42, Stopped) << AnimState( 60, Running) << AnimState(  0, Stopped);
    QTest::newRow("b120-110") << true << 3 << 120 << 110 << AnimState( 42, Stopped) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("b120-100") << true << 3 << 120 << 100 << AnimState(100, Running) << AnimState( 40, Running) << AnimState(  0, Stopped);
    QTest::newRow("b120-50")  << true << 3 << 120 <<  50 << AnimState( 50, Running) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("b120-0")   << true << 3 << 120 <<   0 << AnimState(  0, Stopped) << AnimState(  0, Stopped) << AnimState(  0, Stopped);
    QTest::newRow("b360-170") << true << 3 << 360 << 170 << AnimState( 50, Running) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("b360-220") << true << 3 << 360 << 220 << AnimState(100, Running) << AnimState( 40, Running) << AnimState(  0, Stopped);
    QTest::newRow("b360-210") << true << 3 << 360 << 210 << AnimState( 90, Running) << AnimState( 30, Running) << AnimState(  0, Stopped);
    QTest::newRow("b360-120") << true << 3 << 360 << 120 << AnimState(  0, Stopped) << AnimState( 60, Running) << AnimState(  0, Stopped);

    // rewind, direction = Backward
    QTest::newRow("b50-110")  << true << 3 <<  50 << 110 << AnimState(100, Stopped) << AnimState( 50, Running) << AnimState(  0, Stopped);
    QTest::newRow("b50-120")  << true << 3 <<  50 << 120 << AnimState(100, Stopped) << AnimState( 60, Running) << AnimState(  0, Stopped);
    QTest::newRow("b50-140")  << true << 3 <<  50 << 140 << AnimState( 20, Running) << AnimState( 20, Running) << AnimState(  0, Stopped);
    QTest::newRow("b50-240")  << true << 3 <<  50 << 240 << AnimState(100, Stopped) << AnimState( 60, Running) << AnimState(  0, Stopped);
    QTest::newRow("b50-260")  << true << 3 <<  50 << 260 << AnimState( 20, Running) << AnimState( 20, Running) << AnimState(  0, Stopped);
    QTest::newRow("b50-350")  << true << 3 <<  50 << 350 << AnimState(100, Stopped) << AnimState( 50, Running) << AnimState(  0, Stopped);

    // infinite looping
    QTest::newRow("inf1220")  << false << -1 <<  0 << 1220 << AnimState( 20, Running) << AnimState( 20, Running) << AnimState(  0, Stopped);
    QTest::newRow("inf1310")  << false << -1 <<  0 << 1310 << AnimState( 100, Stopped) << AnimState( 50, Running) << AnimState(  0, Stopped);
    // infinite looping, direction = Backward (will only loop once)
    QTest::newRow("b.inf120-120") << true  << -1 << 120 << 120 << AnimState( 42, Stopped) << AnimState( 60, Running) << AnimState(  0, Stopped);
    QTest::newRow("b.inf120-20")  << true  << -1 << 120 <<  20 << AnimState( 20, Running) << AnimState( 20, Running) << AnimState(  0, Stopped);
    QTest::newRow("b.inf120-110") << true  << -1 << 120 << 110 << AnimState( 42, Stopped) << AnimState( 50, Running) << AnimState(  0, Stopped);


}

#undef Stopped
#undef Running

void tst_QParallelAnimationGroupJob::loopCount()
{
    QFETCH(bool, directionBackward);
    QFETCH(int, setLoopCount);
    QFETCH(int, initialGroupTime);
    QFETCH(int, currentGroupTime);
    QFETCH(AnimState, expected1);
    QFETCH(AnimState, expected2);
    QFETCH(AnimState, expected3);

    QParallelAnimationGroupJob group;

    TestAnimation anim1(100);
    TestAnimation anim2(60);  //total 120
    anim2.setLoopCount(2);
    TestAnimation anim3(0);

    group.appendAnimation(&anim1);
    group.appendAnimation(&anim2);
    group.appendAnimation(&anim3);

    group.setLoopCount(setLoopCount);
    if (initialGroupTime >= 0)
        group.setCurrentTime(initialGroupTime);
    if (directionBackward)
        group.setDirection(QAbstractAnimationJob::Backward);

    group.start();
    if (initialGroupTime >= 0)
        group.setCurrentTime(initialGroupTime);

    anim1.setCurrentTime(42);   // 42 is "untouched"
    anim2.setCurrentTime(42);

    group.setCurrentTime(currentGroupTime);

    QCOMPARE(anim1.currentLoopTime(), expected1.time);
    QCOMPARE(anim2.currentLoopTime(), expected2.time);
    QCOMPARE(anim3.currentLoopTime(), expected3.time);

    if (expected1.state >=0)
        QCOMPARE(int(anim1.state()), expected1.state);
    if (expected2.state >=0)
        QCOMPARE(int(anim2.state()), expected2.state);
    if (expected3.state >=0)
        QCOMPARE(int(anim3.state()), expected3.state);

}

void tst_QParallelAnimationGroupJob::addAndRemoveDuration()
{
    QParallelAnimationGroupJob group;
    QCOMPARE(group.duration(), 0);
    TestAnimation *test = new TestAnimation(250);      // 0, duration = 250;
    group.appendAnimation(test);
    QCOMPARE(test->group(), static_cast<QAnimationGroupJob*>(&group));
    QCOMPARE(test->duration(), 250);
    QCOMPARE(group.duration(), 250);

    TestAnimation *test2 = new TestAnimation(750);     // 1
    group.appendAnimation(test2);
    QCOMPARE(test2->group(), static_cast<QAnimationGroupJob*>(&group));
    QCOMPARE(group.duration(), 750);

    TestAnimation *test3 = new TestAnimation(500);     // 2
    group.appendAnimation(test3);
    QCOMPARE(test3->group(), static_cast<QAnimationGroupJob*>(&group));
    QCOMPARE(group.duration(), 750);

    group.removeAnimation(test2);    // remove the one with duration = 750
    delete test2;
    QCOMPARE(group.duration(), 500);

    group.removeAnimation(test3);    // remove the one with duration = 500
    delete test3;
    QCOMPARE(group.duration(), 250);

    group.removeAnimation(test);    // remove the last one (with duration = 250)
    QCOMPARE(test->group(), static_cast<QAnimationGroupJob*>(0));
    QCOMPARE(group.duration(), 0);
    delete test;
}

void tst_QParallelAnimationGroupJob::pauseResume()
{
#ifdef Q_CC_MINGW
    QSKIP("QTBUG-36290 - MinGW Animation tests are flaky.");
#endif
    QParallelAnimationGroupJob group;
    TestAnimation *anim = new TestAnimation(250);      // 0, duration = 250;
    group.appendAnimation(anim);
    StateChangeListener spy;
    anim->addAnimationChangeListener(&spy, QAbstractAnimationJob::StateChange);
    QCOMPARE(group.duration(), 250);
    group.start();
    QTest::qWait(100);
    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(anim->state(), QAnimationGroupJob::Running);
    QCOMPARE(spy.count(), 1);
    spy.clear();
    const int currentTime = group.currentLoopTime();
    QCOMPARE(anim->currentLoopTime(), currentTime);

    group.pause();
    QCOMPARE(group.state(), QAnimationGroupJob::Paused);
    QCOMPARE(group.currentLoopTime(), currentTime);
    QCOMPARE(anim->state(), QAnimationGroupJob::Paused);
    QCOMPARE(anim->currentLoopTime(), currentTime);
    QCOMPARE(spy.count(), 1);
    spy.clear();

    group.resume();
    QCOMPARE(group.state(), QAnimationGroupJob::Running);
    QCOMPARE(group.currentLoopTime(), currentTime);
    QCOMPARE(anim->state(), QAnimationGroupJob::Running);
    QCOMPARE(anim->currentLoopTime(), currentTime);
    QCOMPARE(spy.count(), 1);

    group.stop();
    spy.clear();
    group.appendAnimation(new TestAnimation(500));
    group.start();
    QCOMPARE(spy.count(), 1); //the animation should have been started
    QCOMPARE(spy.states.at(0), TestAnimation::Running);
    group.setCurrentTime(250); //end of first animation
    QCOMPARE(spy.count(), 2); //the animation should have been stopped
    QCOMPARE(spy.states.at(1), TestAnimation::Stopped);
    group.pause();
    QCOMPARE(spy.count(), 2); //this shouldn't have changed
    group.resume();
    QCOMPARE(spy.count(), 2); //this shouldn't have changed
}

// This is a regression test for QTBUG-8910, where a crash occurred when the
// last animation was removed from a group.
void tst_QParallelAnimationGroupJob::crashWhenRemovingUncontrolledAnimation()
{
    QParallelAnimationGroupJob group;
    TestAnimation *anim = new TestAnimation;
    anim->setLoopCount(-1);
    TestAnimation *anim2 = new TestAnimation;
    anim2->setLoopCount(-1);
    group.appendAnimation(anim);
    group.appendAnimation(anim2);
    group.start();
    delete anim;
    // it would crash here because the internals of the group would still have a reference to anim
    delete anim2;
}

void tst_QParallelAnimationGroupJob::uncontrolledWithLoops()
{
    QParallelAnimationGroupJob group;

    TestAnimation *plain = new TestAnimation(100);
    TestAnimation *loopsForever = new TestAnimation();
    UncontrolledAnimation *notTimeBased = new UncontrolledAnimation();

    loopsForever->setLoopCount(-1);

    group.appendAnimation(plain);
    group.appendAnimation(loopsForever);
    group.appendAnimation(notTimeBased);

    StateChangeListener listener;
    group.addAnimationChangeListener(&listener, QAbstractAnimationJob::CurrentLoop);
    group.setLoopCount(2);

    group.start();

    QCOMPARE(group.currentLoop(), 0);
    QCOMPARE(group.state(), QAbstractAnimationJob::Running);
    QCOMPARE(plain->state(), QAbstractAnimationJob::Running);
    QCOMPARE(loopsForever->state(), QAbstractAnimationJob::Running);
    QCOMPARE(notTimeBased->state(), QAbstractAnimationJob::Running);

    loopsForever->stop();
    notTimeBased->stop();

    QTRY_COMPARE(group.currentLoop(), 1);
    QCOMPARE(group.state(), QAbstractAnimationJob::Running);
    QCOMPARE(loopsForever->state(), QAbstractAnimationJob::Running);
    QCOMPARE(notTimeBased->state(), QAbstractAnimationJob::Running);

    loopsForever->stop();
    notTimeBased->stop();

    QTRY_COMPARE(group.state(), QAbstractAnimationJob::Stopped);
}


QTEST_MAIN(tst_QParallelAnimationGroupJob)
#include "tst_qparallelanimationgroupjob.moc"
