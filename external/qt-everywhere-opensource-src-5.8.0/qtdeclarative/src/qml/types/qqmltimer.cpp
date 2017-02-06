/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qqmltimer_p.h"

#include <QtCore/qcoreapplication.h>
#include "private/qpauseanimationjob_p.h"
#include <qdebug.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

namespace {
    const QEvent::Type QEvent_MaybeTick = QEvent::Type(QEvent::User + 1);
    const QEvent::Type QEvent_Triggered = QEvent::Type(QEvent::User + 2);
}

class QQmlTimerPrivate : public QObjectPrivate, public QAnimationJobChangeListener
{
    Q_DECLARE_PUBLIC(QQmlTimer)
public:
    QQmlTimerPrivate()
        : interval(1000), running(false), repeating(false), triggeredOnStart(false)
        , classBegun(false), componentComplete(false), firstTick(true), awaitingTick(false) {}

    virtual void animationFinished(QAbstractAnimationJob *);
    virtual void animationCurrentLoopChanged(QAbstractAnimationJob *)  { maybeTick(); }

    void maybeTick() {
        Q_Q(QQmlTimer);
        if (!awaitingTick) {
            awaitingTick = true;
            QCoreApplication::postEvent(q, new QEvent(QEvent_MaybeTick));
        }
    }

    int interval;
    QPauseAnimationJob pause;
    bool running : 1;
    bool repeating : 1;
    bool triggeredOnStart : 1;
    bool classBegun : 1;
    bool componentComplete : 1;
    bool firstTick : 1;
    bool awaitingTick : 1;
};

/*!
    \qmltype Timer
    \instantiates QQmlTimer
    \inqmlmodule QtQml
    \ingroup qtquick-interceptors
    \brief Triggers a handler at a specified interval

    A Timer can be used to trigger an action either once, or repeatedly
    at a given interval.

    Here is a Timer that shows the current date and time, and updates
    the text every 500 milliseconds. It uses the JavaScript \c Date
    object to access the current time.

    \qml
    import QtQuick 2.0

    Item {
        Timer {
            interval: 500; running: true; repeat: true
            onTriggered: time.text = Date().toString()
        }

        Text { id: time }
    }
    \endqml

    The Timer type is synchronized with the animation timer.  Since the animation
    timer is usually set to 60fps, the resolution of Timer will be
    at best 16ms.

    If the Timer is running and one of its properties is changed, the
    elapsed time will be reset.  For example, if a Timer with interval of
    1000ms has its \e repeat property changed 500ms after starting, the
    elapsed time will be reset to 0, and the Timer will be triggered
    1000ms later.

    \sa {Qt Quick Demo - Clocks}
*/

QQmlTimer::QQmlTimer(QObject *parent)
    : QObject(*(new QQmlTimerPrivate), parent)
{
    Q_D(QQmlTimer);
    d->pause.addAnimationChangeListener(d, QAbstractAnimationJob::Completion | QAbstractAnimationJob::CurrentLoop);
    d->pause.setLoopCount(1);
    d->pause.setDuration(d->interval);
}

/*!
    \qmlproperty int QtQml::Timer::interval

    Sets the \a interval between triggers, in milliseconds.

    The default interval is 1000 milliseconds.
*/
void QQmlTimer::setInterval(int interval)
{
    Q_D(QQmlTimer);
    if (interval != d->interval) {
        d->interval = interval;
        update();
        emit intervalChanged();
    }
}

int QQmlTimer::interval() const
{
    Q_D(const QQmlTimer);
    return d->interval;
}

/*!
    \qmlproperty bool QtQml::Timer::running

    If set to true, starts the timer; otherwise stops the timer.
    For a non-repeating timer, \a running is set to false after the
    timer has been triggered.

    \a running defaults to false.

    \sa repeat
*/
bool QQmlTimer::isRunning() const
{
    Q_D(const QQmlTimer);
    return d->running;
}

void QQmlTimer::setRunning(bool running)
{
    Q_D(QQmlTimer);
    if (d->running != running) {
        d->running = running;
        d->firstTick = true;
        emit runningChanged();
        update();
    }
}

/*!
    \qmlproperty bool QtQml::Timer::repeat

    If \a repeat is true the timer is triggered repeatedly at the
    specified interval; otherwise, the timer will trigger once at the
    specified interval and then stop (i.e. running will be set to false).

    \a repeat defaults to false.

    \sa running
*/
bool QQmlTimer::isRepeating() const
{
    Q_D(const QQmlTimer);
    return d->repeating;
}

void QQmlTimer::setRepeating(bool repeating)
{
    Q_D(QQmlTimer);
    if (repeating != d->repeating) {
        d->repeating = repeating;
        update();
        emit repeatChanged();
    }
}

/*!
    \qmlproperty bool QtQml::Timer::triggeredOnStart

    When a timer is started, the first trigger is usually after the specified
    interval has elapsed.  It is sometimes desirable to trigger immediately
    when the timer is started; for example, to establish an initial
    state.

    If \a triggeredOnStart is true, the timer is triggered immediately
    when started, and subsequently at the specified interval. Note that if
    \e repeat is set to false, the timer is triggered twice; once on start,
    and again at the interval.

    \a triggeredOnStart defaults to false.

    \sa running
*/
bool QQmlTimer::triggeredOnStart() const
{
    Q_D(const QQmlTimer);
    return d->triggeredOnStart;
}

void QQmlTimer::setTriggeredOnStart(bool triggeredOnStart)
{
    Q_D(QQmlTimer);
    if (d->triggeredOnStart != triggeredOnStart) {
        d->triggeredOnStart = triggeredOnStart;
        update();
        emit triggeredOnStartChanged();
    }
}

/*!
    \qmlmethod QtQml::Timer::start()
    \brief Starts the timer

    If the timer is already running, calling this method has no effect.  The
    \c running property will be true following a call to \c start().
*/
void QQmlTimer::start()
{
    setRunning(true);
}

/*!
    \qmlmethod QtQml::Timer::stop()
    \brief Stops the timer

    If the timer is not running, calling this method has no effect.  The
    \c running property will be false following a call to \c stop().
*/
void QQmlTimer::stop()
{
    setRunning(false);
}

/*!
    \qmlmethod QtQml::Timer::restart()
    \brief Restarts the timer

    If the Timer is not running it will be started, otherwise it will be
    stopped, reset to initial state and started.  The \c running property
    will be true following a call to \c restart().
*/
void QQmlTimer::restart()
{
    setRunning(false);
    setRunning(true);
}

void QQmlTimer::update()
{
    Q_D(QQmlTimer);
    if (d->classBegun && !d->componentComplete)
        return;
    d->pause.stop();
    if (d->running) {
        d->pause.setCurrentTime(0);
        d->pause.setLoopCount(d->repeating ? -1 : 1);
        d->pause.setDuration(d->interval);
        d->pause.start();
        if (d->triggeredOnStart && d->firstTick)
            d->maybeTick();
    }
}

void QQmlTimer::classBegin()
{
    Q_D(QQmlTimer);
    d->classBegun = true;
}

void QQmlTimer::componentComplete()
{
    Q_D(QQmlTimer);
    d->componentComplete = true;
    update();
}

/*!
    \qmlsignal QtQml::Timer::triggered()

    This signal is emitted when the Timer times out.

    The corresponding handler is \c onTriggered.
*/
void QQmlTimer::ticked()
{
    Q_D(QQmlTimer);
    if (d->running && (d->pause.currentTime() > 0 || (d->triggeredOnStart && d->firstTick)))
        emit triggered();
    d->firstTick = false;
}

/*!
    \internal
 */
bool QQmlTimer::event(QEvent *e)
{
    Q_D(QQmlTimer);
    if (e->type() == QEvent_MaybeTick) {
        d->awaitingTick = false;
        ticked();
        return true;
    } else if (e->type() == QEvent_Triggered) {
        if (d->running && d->pause.isStopped()) {
            d->running = false;
            emit triggered();
            emit runningChanged();
        }
        return true;
    }
    return QObject::event(e);
}

void QQmlTimerPrivate::animationFinished(QAbstractAnimationJob *)
{
    Q_Q(QQmlTimer);
    if (repeating || !running)
        return;
    firstTick = false;
    QCoreApplication::postEvent(q, new QEvent(QEvent_Triggered));
}

QT_END_NAMESPACE
