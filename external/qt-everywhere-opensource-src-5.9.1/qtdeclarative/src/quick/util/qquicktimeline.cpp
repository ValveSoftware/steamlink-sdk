/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquicktimeline_p_p.h"

#include <QDebug>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <QEvent>
#include <QCoreApplication>
#include <QEasingCurve>
#include <QTime>
#include <QtCore/private/qnumeric_p.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

struct Update {
    Update(QQuickTimeLineValue *_g, qreal _v)
        : g(_g), v(_v) {}
    Update(const QQuickTimeLineCallback &_e)
        : g(0), v(0), e(_e) {}

    QQuickTimeLineValue *g;
    qreal v;
    QQuickTimeLineCallback e;
};

struct QQuickTimeLinePrivate
{
    QQuickTimeLinePrivate(QQuickTimeLine *);

    struct Op {
        enum Type {
            Pause,
            Set,
            Move,
            MoveBy,
            Accel,
            AccelDistance,
            Execute
        };
        Op() {}
        Op(Type t, int l, qreal v, qreal v2, int o,
           const QQuickTimeLineCallback &ev = QQuickTimeLineCallback(), const QEasingCurve &es = QEasingCurve())
            : type(t), length(l), value(v), value2(v2), order(o), event(ev),
              easing(es) {}
        Op(const Op &o)
            : type(o.type), length(o.length), value(o.value), value2(o.value2),
              order(o.order), event(o.event), easing(o.easing) {}
        Op &operator=(const Op &o) {
            type = o.type; length = o.length; value = o.value;
            value2 = o.value2; order = o.order; event = o.event;
            easing = o.easing;
            return *this;
        }

        Type type;
        int length;
        qreal value;
        qreal value2;

        int order;
        QQuickTimeLineCallback event;
        QEasingCurve easing;
    };
    struct TimeLine
    {
        TimeLine() : length(0), consumedOpLength(0), base(0.) {}
        QList<Op> ops;
        int length;
        int consumedOpLength;
        qreal base;
    };

    int length;
    int syncPoint;
    typedef QHash<QQuickTimeLineObject *, TimeLine> Ops;
    Ops ops;
    QQuickTimeLine *q;

    void add(QQuickTimeLineObject &, const Op &);
    qreal value(const Op &op, int time, qreal base, bool *) const;

    int advance(int);

    bool clockRunning;
    int prevTime;

    int order;

    QQuickTimeLine::SyncMode syncMode;
    int syncAdj;
    QList<QPair<int, Update> > *updateQueue;
};

QQuickTimeLinePrivate::QQuickTimeLinePrivate(QQuickTimeLine *parent)
: length(0), syncPoint(0), q(parent), clockRunning(false), prevTime(0), order(0), syncMode(QQuickTimeLine::LocalSync), syncAdj(0), updateQueue(0)
{
}

void QQuickTimeLinePrivate::add(QQuickTimeLineObject &g, const Op &o)
{
    if (g._t && g._t != q) {
        qWarning() << "QQuickTimeLine: Cannot modify a QQuickTimeLineValue owned by"
                   << "another timeline.";
        return;
    }
    g._t = q;

    Ops::Iterator iter = ops.find(&g);
    if (iter == ops.end()) {
        iter = ops.insert(&g, TimeLine());
        if (syncPoint > 0)
            q->pause(g, syncPoint);
    }
    if (!iter->ops.isEmpty() &&
       o.type == Op::Pause &&
       iter->ops.constLast().type == Op::Pause) {
        iter->ops.last().length += o.length;
        iter->length += o.length;
    } else {
        iter->ops.append(o);
        iter->length += o.length;
    }

    if (iter->length > length)
        length = iter->length;

    if (!clockRunning) {
        q->stop();
        prevTime = 0;
        clockRunning = true;

        if (syncMode == QQuickTimeLine::LocalSync)  {
            syncAdj = -1;
        } else {
            syncAdj = 0;
        }
        q->start();
/*        q->tick(0);
        if (syncMode == QQuickTimeLine::LocalSync)  {
            syncAdj = -1;
        } else {
            syncAdj = 0;
        }
        */
    }
}

qreal QQuickTimeLinePrivate::value(const Op &op, int time, qreal base, bool *changed) const
{
    Q_ASSERT(time >= 0);
    Q_ASSERT(time <= op.length);
    *changed = true;

    switch(op.type) {
        case Op::Pause:
            *changed = false;
            return base;
        case Op::Set:
            return op.value;
        case Op::Move:
            if (time == 0) {
                return base;
            } else if (time == (op.length)) {
                return op.value;
            } else {
                qreal delta = op.value - base;
                qreal pTime = (qreal)(time) / (qreal)op.length;
                if (op.easing.type() == QEasingCurve::Linear)
                    return base + delta * pTime;
                else
                    return base + delta * op.easing.valueForProgress(pTime);
            }
        case Op::MoveBy:
            if (time == 0) {
                return base;
            } else if (time == (op.length)) {
                return base + op.value;
            } else {
                qreal delta = op.value;
                qreal pTime = (qreal)(time) / (qreal)op.length;
                if (op.easing.type() == QEasingCurve::Linear)
                    return base + delta * pTime;
                else
                    return base + delta * op.easing.valueForProgress(pTime);
            }
        case Op::Accel:
            if (time == 0) {
                return base;
            } else {
                qreal t = (qreal)(time) / 1000.0f;
                qreal delta = op.value * t + 0.5f * op.value2 * t * t;
                return base + delta;
            }
        case Op::AccelDistance:
            if (time == 0) {
                return base;
            } else if (time == (op.length)) {
                return base + op.value2;
            } else {
                qreal t = (qreal)(time) / 1000.0f;
                qreal accel = -1.0f * 1000.0f * op.value / (qreal)op.length;
                qreal delta = op.value * t + 0.5f * accel * t * t;
                return base + delta;

            }
        case Op::Execute:
            op.event.d0(op.event.d1);
            *changed = false;
            return -1;
    }

    return base;
}

/*!
    \internal
    \class QQuickTimeLine
    \brief The QQuickTimeLine class provides a timeline for controlling animations.

    QQuickTimeLine is similar to QTimeLine except:
    \list
    \li It updates QQuickTimeLineValue instances directly, rather than maintaining a single
    current value.

    For example, the following animates a simple value over 200 milliseconds:
    \code
    QQuickTimeLineValue v(<starting value>);
    QQuickTimeLine tl;
    tl.move(v, 100., 200);
    tl.start()
    \endcode

    If your program needs to know when values are changed, it can either
    connect to the QQuickTimeLine's updated() signal, or inherit from QQuickTimeLineValue
    and reimplement the QQuickTimeLineValue::setValue() method.

    \li Supports multiple QQuickTimeLineValue, arbitrary start and end values and allows
    animations to be strung together for more complex effects.

    For example, the following animation moves the x and y coordinates of
    an object from wherever they are to the position (100, 100) in 50
    milliseconds and then further animates them to (100, 200) in 50
    milliseconds:

    \code
    QQuickTimeLineValue x(<starting value>);
    QQuickTimeLineValue y(<starting value>);

    QQuickTimeLine tl;
    tl.start();

    tl.move(x, 100., 50);
    tl.move(y, 100., 50);
    tl.move(y, 200., 50);
    \endcode

    \li All QQuickTimeLine instances share a single, synchronized clock.

    Actions scheduled within the same event loop tick are scheduled
    synchronously against each other, regardless of the wall time between the
    scheduling.  Synchronized scheduling applies both to within the same
    QQuickTimeLine and across separate QQuickTimeLine's within the same process.

    \endlist

    Currently easing functions are not supported.
*/


/*!
    Construct a new QQuickTimeLine with the specified \a parent.
*/
QQuickTimeLine::QQuickTimeLine(QObject *parent)
    : QObject(parent)
{
    d = new QQuickTimeLinePrivate(this);
}

/*!
    Destroys the time line.  Any inprogress animations are canceled, but not
    completed.
*/
QQuickTimeLine::~QQuickTimeLine()
{
    for (QQuickTimeLinePrivate::Ops::Iterator iter = d->ops.begin();
            iter != d->ops.end();
            ++iter)
        iter.key()->_t = 0;

    delete d; d = 0;
}

/*!
    \enum QQuickTimeLine::SyncMode
 */

/*!
    Return the timeline's synchronization mode.
 */
QQuickTimeLine::SyncMode QQuickTimeLine::syncMode() const
{
    return d->syncMode;
}

/*!
    Set the timeline's synchronization mode to \a syncMode.
 */
void QQuickTimeLine::setSyncMode(SyncMode syncMode)
{
    d->syncMode = syncMode;
}

/*!
    Pause \a obj for \a time milliseconds.
*/
void QQuickTimeLine::pause(QQuickTimeLineObject &obj, int time)
{
    if (time <= 0) return;
    QQuickTimeLinePrivate::Op op(QQuickTimeLinePrivate::Op::Pause, time, 0., 0., d->order++);
    d->add(obj, op);
}

/*!
    Execute the \a event.
 */
void QQuickTimeLine::callback(const QQuickTimeLineCallback &callback)
{
    QQuickTimeLinePrivate::Op op(QQuickTimeLinePrivate::Op::Execute, 0, 0, 0., d->order++, callback);
    d->add(*callback.callbackObject(), op);
}

/*!
    Set the \a value of \a timeLineValue.
*/
void QQuickTimeLine::set(QQuickTimeLineValue &timeLineValue, qreal value)
{
    QQuickTimeLinePrivate::Op op(QQuickTimeLinePrivate::Op::Set, 0, value, 0., d->order++);
    d->add(timeLineValue, op);
}

/*!
    Decelerate \a timeLineValue from the starting \a velocity to zero at the
    given \a acceleration rate.  Although the \a acceleration is technically
    a deceleration, it should always be positive.  The QQuickTimeLine will ensure
    that the deceleration is in the opposite direction to the initial velocity.
*/
int QQuickTimeLine::accel(QQuickTimeLineValue &timeLineValue, qreal velocity, qreal acceleration)
{
    if (qFuzzyIsNull(acceleration) || qt_is_nan(acceleration))
        return -1;

    if ((velocity > 0.0f) == (acceleration > 0.0f))
        acceleration = acceleration * -1.0f;

    int time = static_cast<int>(-1000 * velocity / acceleration);
    if (time <= 0) return -1;

    QQuickTimeLinePrivate::Op op(QQuickTimeLinePrivate::Op::Accel, time, velocity, acceleration, d->order++);
    d->add(timeLineValue, op);

    return time;
}

/*!
    \overload

    Decelerate \a timeLineValue from the starting \a velocity to zero at the
    given \a acceleration rate over a maximum distance of maxDistance.

    If necessary, QQuickTimeLine will reduce the acceleration to ensure that the
    entire operation does not require a move of more than \a maxDistance.
    \a maxDistance should always be positive.
*/
int QQuickTimeLine::accel(QQuickTimeLineValue &timeLineValue, qreal velocity, qreal acceleration, qreal maxDistance)
{
    if (qFuzzyIsNull(maxDistance) || qt_is_nan(maxDistance) || qFuzzyIsNull(acceleration) || qt_is_nan(acceleration))
        return -1;

    Q_ASSERT(acceleration > 0.0f && maxDistance > 0.0f);

    qreal maxAccel = (velocity * velocity) / (2.0f * maxDistance);
    if (maxAccel > acceleration)
        acceleration = maxAccel;

    if ((velocity > 0.0f) == (acceleration > 0.0f))
        acceleration = acceleration * -1.0f;

    int time = static_cast<int>(-1000 * velocity / acceleration);
    if (time <= 0) return -1;

    QQuickTimeLinePrivate::Op op(QQuickTimeLinePrivate::Op::Accel, time, velocity, acceleration, d->order++);
    d->add(timeLineValue, op);

    return time;
}

/*!
    Decelerate \a timeLineValue from the starting \a velocity to zero over the given
    \a distance.  This is like accel(), but the QQuickTimeLine calculates the exact
    deceleration to use.

    \a distance should be positive.
*/
int QQuickTimeLine::accelDistance(QQuickTimeLineValue &timeLineValue, qreal velocity, qreal distance)
{
    if (qFuzzyIsNull(distance) || qt_is_nan(distance) || qFuzzyIsNull(velocity) || qt_is_nan(velocity))
        return -1;

    Q_ASSERT((distance >= 0.0f) == (velocity >= 0.0f));

    int time = static_cast<int>(1000 * (2.0f * distance) / velocity);
    if (time <= 0) return -1;

    QQuickTimeLinePrivate::Op op(QQuickTimeLinePrivate::Op::AccelDistance, time, velocity, distance, d->order++);
    d->add(timeLineValue, op);

    return time;
}

/*!
    Linearly change the \a timeLineValue from its current value to the given
    \a destination value over \a time milliseconds.
*/
void QQuickTimeLine::move(QQuickTimeLineValue &timeLineValue, qreal destination, int time)
{
    if (time <= 0) return;
    QQuickTimeLinePrivate::Op op(QQuickTimeLinePrivate::Op::Move, time, destination, 0.0f, d->order++);
    d->add(timeLineValue, op);
}

/*!
    Change the \a timeLineValue from its current value to the given \a destination
    value over \a time milliseconds using the \a easing curve.
 */
void QQuickTimeLine::move(QQuickTimeLineValue &timeLineValue, qreal destination, const QEasingCurve &easing, int time)
{
    if (time <= 0) return;
    QQuickTimeLinePrivate::Op op(QQuickTimeLinePrivate::Op::Move, time, destination, 0.0f, d->order++, QQuickTimeLineCallback(), easing);
    d->add(timeLineValue, op);
}

/*!
    Linearly change the \a timeLineValue from its current value by the \a change amount
    over \a time milliseconds.
*/
void QQuickTimeLine::moveBy(QQuickTimeLineValue &timeLineValue, qreal change, int time)
{
    if (time <= 0) return;
    QQuickTimeLinePrivate::Op op(QQuickTimeLinePrivate::Op::MoveBy, time, change, 0.0f, d->order++);
    d->add(timeLineValue, op);
}

/*!
    Change the \a timeLineValue from its current value by the \a change amount over
    \a time milliseconds using the \a easing curve.
 */
void QQuickTimeLine::moveBy(QQuickTimeLineValue &timeLineValue, qreal change, const QEasingCurve &easing, int time)
{
    if (time <= 0) return;
    QQuickTimeLinePrivate::Op op(QQuickTimeLinePrivate::Op::MoveBy, time, change, 0.0f, d->order++, QQuickTimeLineCallback(), easing);
    d->add(timeLineValue, op);
}

/*!
    Cancel (but don't complete) all scheduled actions for \a timeLineValue.
*/
void QQuickTimeLine::reset(QQuickTimeLineValue &timeLineValue)
{
    if (!timeLineValue._t)
        return;
    if (timeLineValue._t != this) {
        qWarning() << "QQuickTimeLine: Cannot reset a QQuickTimeLineValue owned by another timeline.";
        return;
    }
    remove(&timeLineValue);
    timeLineValue._t = 0;
}

int QQuickTimeLine::duration() const
{
    return -1;
}

/*!
    Synchronize the end point of \a timeLineValue to the endpoint of \a syncTo
    within this timeline.

    Following operations on \a timeLineValue in this timeline will be scheduled after
    all the currently scheduled actions on \a syncTo are complete.  In
    pseudo-code this is equivalent to:
    \code
    QQuickTimeLine::pause(timeLineValue, min(0, length_of(syncTo) - length_of(timeLineValue)))
    \endcode
*/
void QQuickTimeLine::sync(QQuickTimeLineValue &timeLineValue, QQuickTimeLineValue &syncTo)
{
    QQuickTimeLinePrivate::Ops::Iterator iter = d->ops.find(&syncTo);
    if (iter == d->ops.end())
        return;
    int length = iter->length;

    iter = d->ops.find(&timeLineValue);
    if (iter == d->ops.end()) {
        pause(timeLineValue, length);
    } else {
        int glength = iter->length;
        pause(timeLineValue, length - glength);
    }
}

/*!
    Synchronize the end point of \a timeLineValue to the endpoint of the longest
    action cursrently scheduled in the timeline.

    In pseudo-code, this is equivalent to:
    \code
    QQuickTimeLine::pause(timeLineValue, length_of(timeline) - length_of(timeLineValue))
    \endcode
*/
void QQuickTimeLine::sync(QQuickTimeLineValue &timeLineValue)
{
    QQuickTimeLinePrivate::Ops::Iterator iter = d->ops.find(&timeLineValue);
    if (iter == d->ops.end()) {
        pause(timeLineValue, d->length);
    } else {
        pause(timeLineValue, d->length - iter->length);
    }
}

/*
    Synchronize all currently and future scheduled values in this timeline to
    the longest action currently scheduled.

    For example:
    \code
    value1->setValue(0.);
    value2->setValue(0.);
    value3->setValue(0.);
    QQuickTimeLine tl;
    ...
    tl.move(value1, 10, 200);
    tl.move(value2, 10, 100);
    tl.sync();
    tl.move(value2, 20, 100);
    tl.move(value3, 20, 100);
    \endcode

    will result in:

    \table
    \header \li \li 0ms \li 50ms \li 100ms \li 150ms \li 200ms \li 250ms \li 300ms
    \row \li value1 \li 0 \li 2.5 \li 5.0 \li 7.5 \li 10 \li 10 \li 10
    \row \li value2 \li 0 \li 5.0 \li 10.0 \li 10.0 \li 10.0 \li 15.0 \li 20.0
    \row \li value2 \li 0 \li 0 \li 0 \li 0 \li 0 \li 10.0 \li 20.0
    \endtable
*/

/*void QQuickTimeLine::sync()
{
    for (QQuickTimeLinePrivate::Ops::Iterator iter = d->ops.begin();
            iter != d->ops.end();
            ++iter)
        pause(*iter.key(), d->length - iter->length);
    d->syncPoint = d->length;
}*/

/*!
    \internal

    Temporary hack.
 */
void QQuickTimeLine::setSyncPoint(int sp)
{
    d->syncPoint = sp;
}

/*!
    \internal

    Temporary hack.
 */
int QQuickTimeLine::syncPoint() const
{
    return d->syncPoint;
}

/*!
    Returns true if the timeline is active.  An active timeline is one where
    QQuickTimeLineValue actions are still pending.
*/
bool QQuickTimeLine::isActive() const
{
    return !d->ops.isEmpty();
}

/*!
    Completes the timeline.  All queued actions are played to completion, and then discarded.  For example,
    \code
    QQuickTimeLineValue v(0.);
    QQuickTimeLine tl;
    tl.move(v, 100., 1000.);
    // 500 ms passes
    // v.value() == 50.
    tl.complete();
    // v.value() == 100.
    \endcode
*/
void QQuickTimeLine::complete()
{
    d->advance(d->length);
}

/*!
    Resets the timeline.  All queued actions are discarded and QQuickTimeLineValue's retain their current value. For example,
    \code
    QQuickTimeLineValue v(0.);
    QQuickTimeLine tl;
    tl.move(v, 100., 1000.);
    // 500 ms passes
    // v.value() == 50.
    tl.clear();
    // v.value() == 50.
    \endcode
*/
void QQuickTimeLine::clear()
{
    for (QQuickTimeLinePrivate::Ops::const_iterator iter = d->ops.cbegin(), cend  = d->ops.cend(); iter != cend; ++iter)
        iter.key()->_t = 0;
    d->ops.clear();
    d->length = 0;
    d->syncPoint = 0;
    //XXX need stop here?
}

int QQuickTimeLine::time() const
{
    return d->prevTime;
}

/*!
    \fn void QQuickTimeLine::updated()

    Emitted each time the timeline modifies QQuickTimeLineValues.  Even if multiple
    QQuickTimeLineValues are changed, this signal is only emitted once for each clock tick.
*/

void QQuickTimeLine::updateCurrentTime(int v)
{
    if (d->syncAdj == -1)
        d->syncAdj = v;
    v -= d->syncAdj;

    int timeChanged = v - d->prevTime;
#if 0
    if (!timeChanged)
        return;
#endif
    d->prevTime = v;
    d->advance(timeChanged);
    emit updated();

    // Do we need to stop the clock?
    if (d->ops.isEmpty()) {
        stop();
        d->prevTime = 0;
        d->clockRunning = false;
        emit completed();
    } /*else if (pauseTime > 0) {
        GfxClock::cancelClock();
        d->prevTime = 0;
        GfxClock::pauseFor(pauseTime);
        d->syncAdj = 0;
        d->clockRunning = false;
    }*/ else if (/*!GfxClock::isActive()*/ state() != Running) {
        stop();
        d->prevTime = 0;
        d->clockRunning = true;
        d->syncAdj = 0;
        start();
    }
}

void QQuickTimeLine::debugAnimation(QDebug d) const
{
    d << "QuickTimeLine(" << hex << (const void *) this << dec << ")";
}

bool operator<(const QPair<int, Update> &lhs,
               const QPair<int, Update> &rhs)
{
    return lhs.first < rhs.first;
}

int QQuickTimeLinePrivate::advance(int t)
{
    int pauseTime = -1;

    // XXX - surely there is a more efficient way?
    do {
        pauseTime = -1;
        // Minimal advance time
        int advanceTime = t;
        for (Ops::const_iterator iter = ops.constBegin(), cend = ops.constEnd(); iter != cend; ++iter) {
            const TimeLine &tl = *iter;
            const Op &op = tl.ops.first();
            int length = op.length - tl.consumedOpLength;

            if (length < advanceTime) {
                advanceTime = length;
                if (advanceTime == 0)
                    break;
            }
        }
        t -= advanceTime;

        // Process until then.  A zero length advance time will only process
        // sets.
        QList<QPair<int, Update> > updates;

        for (Ops::Iterator iter = ops.begin(); iter != ops.end(); ) {
            QQuickTimeLineValue *v = static_cast<QQuickTimeLineValue *>(iter.key());
            TimeLine &tl = *iter;
            Q_ASSERT(!tl.ops.isEmpty());

            do {
                Op &op = tl.ops.first();
                if (advanceTime == 0 && op.length != 0)
                    continue;

                if (tl.consumedOpLength == 0 &&
                   op.type != Op::Pause &&
                   op.type != Op::Execute)
                    tl.base = v->value();

                if ((tl.consumedOpLength + advanceTime) == op.length) {
                    if (op.type == Op::Execute) {
                        updates << qMakePair(op.order, Update(op.event));
                    } else {
                        bool changed = false;
                        qreal val = value(op, op.length, tl.base, &changed);
                        if (changed)
                            updates << qMakePair(op.order, Update(v, val));
                    }
                    tl.length -= qMin(advanceTime, tl.length);
                    tl.consumedOpLength = 0;
                    tl.ops.removeFirst();
                } else {
                    tl.consumedOpLength += advanceTime;
                    bool changed = false;
                    qreal val = value(op, tl.consumedOpLength, tl.base, &changed);
                    if (changed)
                        updates << qMakePair(op.order, Update(v, val));
                    tl.length -= qMin(advanceTime, tl.length);
                    break;
                }

            } while(!tl.ops.isEmpty() && advanceTime == 0 && tl.ops.first().length == 0);


            if (tl.ops.isEmpty()) {
                iter = ops.erase(iter);
                v->_t = 0;
            } else {
                if (tl.ops.first().type == Op::Pause && pauseTime != 0) {
                    int opPauseTime = tl.ops.first().length - tl.consumedOpLength;
                    if (pauseTime == -1 || opPauseTime < pauseTime)
                        pauseTime = opPauseTime;
                } else {
                    pauseTime = 0;
                }
                ++iter;
            }
        }

        length -= qMin(length, advanceTime);
        syncPoint -= advanceTime;

        std::sort(updates.begin(), updates.end());
        updateQueue = &updates;
        for (int ii = 0; ii < updates.count(); ++ii) {
            const Update &v = updates.at(ii).second;
            if (v.g) {
                v.g->setValue(v.v);
            } else {
                v.e.d0(v.e.d1);
            }
        }
        updateQueue = 0;
    } while(t);

    return pauseTime;
}

void QQuickTimeLine::remove(QQuickTimeLineObject *v)
{
    QQuickTimeLinePrivate::Ops::Iterator iter = d->ops.find(v);
    Q_ASSERT(iter != d->ops.end());

    int len = iter->length;
    d->ops.erase(iter);
    if (len == d->length) {
        // We need to recalculate the length
        d->length = 0;
        for (QQuickTimeLinePrivate::Ops::Iterator iter = d->ops.begin();
                iter != d->ops.end();
                ++iter) {

            if (iter->length > d->length)
                d->length = iter->length;

        }
    }
    if (d->ops.isEmpty()) {
        stop();
        d->clockRunning = false;
    } else if (/*!GfxClock::isActive()*/ state() != Running) {
        stop();
        d->prevTime = 0;
        d->clockRunning = true;

        if (d->syncMode == QQuickTimeLine::LocalSync) {
            d->syncAdj = -1;
        } else {
            d->syncAdj = 0;
        }
        start();
    }

    if (d->updateQueue) {
        for (int ii = 0; ii < d->updateQueue->count(); ++ii) {
            if (d->updateQueue->at(ii).second.g == v ||
               d->updateQueue->at(ii).second.e.callbackObject() == v) {
                d->updateQueue->removeAt(ii);
                --ii;
            }
        }
    }


}

/*!
    \internal
    \class QQuickTimeLineValue
    \brief The QQuickTimeLineValue class provides a value that can be modified by QQuickTimeLine.
*/

/*!
    \fn QQuickTimeLineValue::QQuickTimeLineValue(qreal value = 0)

    Construct a new QQuickTimeLineValue with an initial \a value.
*/

/*!
    \fn qreal QQuickTimeLineValue::value() const

    Return the current value.
*/

/*!
    \fn void QQuickTimeLineValue::setValue(qreal value)

    Set the current \a value.
*/

/*!
    \fn QQuickTimeLine *QQuickTimeLineValue::timeLine() const

    If a QQuickTimeLine is operating on this value, return a pointer to it,
    otherwise return null.
*/


QQuickTimeLineObject::QQuickTimeLineObject()
: _t(0)
{
}

QQuickTimeLineObject::~QQuickTimeLineObject()
{
    if (_t) {
        _t->remove(this);
        _t = 0;
    }
}

QQuickTimeLineCallback::QQuickTimeLineCallback()
: d0(0), d1(0), d2(0)
{
}

QQuickTimeLineCallback::QQuickTimeLineCallback(QQuickTimeLineObject *b, Callback f, void *d)
: d0(f), d1(d), d2(b)
{
}

QQuickTimeLineCallback::QQuickTimeLineCallback(const QQuickTimeLineCallback &o)
: d0(o.d0), d1(o.d1), d2(o.d2)
{
}

QQuickTimeLineCallback &QQuickTimeLineCallback::operator=(const QQuickTimeLineCallback &o)
{
    d0 = o.d0;
    d1 = o.d1;
    d2 = o.d2;
    return *this;
}

QQuickTimeLineObject *QQuickTimeLineCallback::callbackObject() const
{
    return d2;
}

QT_END_NAMESPACE
