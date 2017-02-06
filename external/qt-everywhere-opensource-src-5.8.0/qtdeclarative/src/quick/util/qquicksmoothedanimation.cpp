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

#include "qquicksmoothedanimation_p.h"
#include "qquicksmoothedanimation_p_p.h"

#include "qquickanimation_p_p.h"
#include "private/qcontinuinganimationgroupjob_p.h"

#include <qmath.h>
#include <qqmlproperty.h>
#include <private/qqmlproperty_p.h>

#include <private/qqmlglobal_p.h>

#include <QtCore/qdebug.h>


#define DELAY_STOP_TIMER_INTERVAL 32

QT_BEGIN_NAMESPACE


QSmoothedAnimationTimer::QSmoothedAnimationTimer(QSmoothedAnimation *animation, QObject *parent)
    : QTimer(parent)
    , m_animation(animation)
{
    connect(this, SIGNAL(timeout()), this, SLOT(stopAnimation()));
}

QSmoothedAnimationTimer::~QSmoothedAnimationTimer()
{
}

void QSmoothedAnimationTimer::stopAnimation()
{
    m_animation->stop();
}

QSmoothedAnimation::QSmoothedAnimation(QQuickSmoothedAnimationPrivate *priv)
    : QAbstractAnimationJob(), to(0), velocity(200), userDuration(-1), maximumEasingTime(-1),
      reversingMode(QQuickSmoothedAnimation::Eased), initialVelocity(0),
      trackVelocity(0), initialValue(0), invert(false), finalDuration(-1), lastTime(0),
      skipUpdate(false), delayedStopTimer(new QSmoothedAnimationTimer(this)), animationTemplate(priv)
{
    delayedStopTimer->setInterval(DELAY_STOP_TIMER_INTERVAL);
    delayedStopTimer->setSingleShot(true);
}

QSmoothedAnimation::~QSmoothedAnimation()
{
    delete delayedStopTimer;
    if (animationTemplate) {
        if (target.object()) {
            QHash<QQmlProperty, QSmoothedAnimation* >::iterator it =
                    animationTemplate->activeAnimations.find(target);
            if (it != animationTemplate->activeAnimations.end() && it.value() == this)
                animationTemplate->activeAnimations.erase(it);
        } else {
            //target is no longer valid, need to search linearly
            QHash<QQmlProperty, QSmoothedAnimation* >::iterator it;
            for (it = animationTemplate->activeAnimations.begin(); it != animationTemplate->activeAnimations.end(); ++it) {
                if (it.value() == this) {
                    animationTemplate->activeAnimations.erase(it);
                    break;
                }
            }
        }
    }
}

void QSmoothedAnimation::restart()
{
    initialVelocity = trackVelocity;
    if (isRunning())
        init();
    else
        start();
}

void QSmoothedAnimation::prepareForRestart()
{
    initialVelocity = trackVelocity;
    if (isRunning()) {
        //we are joining a new wrapper group while running, our times need to be restarted
        skipUpdate = true;
        init();
        lastTime = 0;
    } else {
        skipUpdate = false;
        //we'll be started when the group starts, which will force an init()
    }
}

void QSmoothedAnimation::updateState(QAbstractAnimationJob::State newState, QAbstractAnimationJob::State /*oldState*/)
{
    if (newState == QAbstractAnimationJob::Running)
        init();
}

void QSmoothedAnimation::delayedStop()
{
    if (!delayedStopTimer->isActive())
        delayedStopTimer->start();
}

int QSmoothedAnimation::duration() const
{
    return -1;
}

bool QSmoothedAnimation::recalc()
{
    s = to - initialValue;
    vi = initialVelocity;

    s = (invert? -1.0: 1.0) * s;

    if (userDuration >= 0 && velocity > 0) {
        tf = s / velocity;
        if (tf > (userDuration / 1000.)) tf = (userDuration / 1000.);
    } else if (userDuration >= 0) {
        tf = userDuration / 1000.;
    } else if (velocity > 0) {
        tf = s / velocity;
    } else {
        return false;
    }

    finalDuration = qCeil(tf * 1000.0);

    if (maximumEasingTime == 0) {
        a = 0;
        d = 0;
        tp = 0;
        td = tf;
        vp = velocity;
        sp = 0;
        sd = s;
    } else if (maximumEasingTime != -1 && tf > (maximumEasingTime / 1000.)) {
        qreal met = maximumEasingTime / 1000.;
        /*       tp|       |td
         * vp_      _______
         *         /       \
         * vi_    /         \
         *                   \
         *                    \   _ 0
         *       |ta|      |ta|
         */
        qreal ta = met / 2.;
        a = (s - (vi * tf - 0.5 * vi * ta)) / (tf * ta - ta * ta);

        vp = vi + a * ta;
        d = vp / ta;
        tp = ta;
        sp = vi * ta + 0.5 * a * tp * tp;
        sd = sp + vp * (tf - 2 * ta);
        td = tf - ta;
    } else {
        qreal c1 = 0.25 * tf * tf;
        qreal c2 = 0.5 * vi * tf - s;
        qreal c3 = -0.25 * vi * vi;

        qreal a1 = (-c2 + qSqrt(c2 * c2 - 4 * c1 * c3)) / (2. * c1);

        qreal tp1 = 0.5 * tf - 0.5 * vi / a1;
        qreal vp1 = a1 * tp1 + vi;

        qreal sp1 = 0.5 * a1 * tp1 * tp1 + vi * tp1;

        a = a1;
        d = a1;
        tp = tp1;
        td = tp1;
        vp = vp1;
        sp = sp1;
        sd = sp1;
    }
    return true;
}

qreal QSmoothedAnimation::easeFollow(qreal time_seconds)
{
    qreal value;
    if (time_seconds < tp) {
        trackVelocity = vi + time_seconds * a;
        value = 0.5 * a * time_seconds * time_seconds + vi * time_seconds;
    } else if (time_seconds < td) {
        time_seconds -= tp;
        trackVelocity = vp;
        value = sp + time_seconds * vp;
    } else if (time_seconds < tf) {
        time_seconds -= td;
        trackVelocity = vp - time_seconds * a;
        value = sd - 0.5 * d * time_seconds * time_seconds + vp * time_seconds;
    } else {
        trackVelocity = 0;
        value = s;
        delayedStop();
    }

    // to normalize 's' between [0..1], divide 'value' by 's'
    return value;
}

void QSmoothedAnimation::updateCurrentTime(int t)
{
    if (skipUpdate) {
        skipUpdate = false;
        return;
    }

    if (!isRunning() && !isPaused()) // This can happen if init() stops the animation in some cases
        return;

    qreal time_seconds = qreal(t - lastTime) / 1000.;

    qreal value = easeFollow(time_seconds);
    value *= (invert? -1.0: 1.0);
    QQmlPropertyPrivate::write(target, initialValue + value,
                                       QQmlPropertyData::BypassInterceptor
                                       | QQmlPropertyData::DontRemoveBinding);
}

void QSmoothedAnimation::init()
{
    if (velocity == 0) {
        stop();
        return;
    }

    if (delayedStopTimer->isActive())
        delayedStopTimer->stop();

    initialValue = target.read().toReal();
    lastTime = this->currentTime();

    if (to == initialValue) {
        stop();
        return;
    }

    bool hasReversed = trackVelocity != 0. &&
                      ((!invert) == ((initialValue - to) > 0));

    if (hasReversed) {
        switch (reversingMode) {
            default:
            case QQuickSmoothedAnimation::Eased:
                initialVelocity = -trackVelocity;
                break;
            case QQuickSmoothedAnimation::Sync:
                QQmlPropertyPrivate::write(target, to,
                                                   QQmlPropertyData::BypassInterceptor
                                                   | QQmlPropertyData::DontRemoveBinding);
                trackVelocity = 0;
                stop();
                return;
            case QQuickSmoothedAnimation::Immediate:
                initialVelocity = 0;
                break;
        }
    }

    trackVelocity = initialVelocity;

    invert = (to < initialValue);

    if (!recalc()) {
        QQmlPropertyPrivate::write(target, to,
                                           QQmlPropertyData::BypassInterceptor
                                           | QQmlPropertyData::DontRemoveBinding);
        stop();
        return;
    }
}

void QSmoothedAnimation::debugAnimation(QDebug d) const
{
    d << "SmoothedAnimationJob(" << hex << (const void *) this << dec << ")" << "duration:" << userDuration
      << "velocity:" << velocity << "target:" << target.object() << "property:" << target.name()
      << "to:" << to << "current velocity:" << trackVelocity;
}

/*!
    \qmltype SmoothedAnimation
    \instantiates QQuickSmoothedAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-transitions-animations
    \inherits NumberAnimation
    \brief Allows a property to smoothly track a value

    A SmoothedAnimation animates a property's value to a set target value
    using an ease in/out quad easing curve.  When the target value changes,
    the easing curves used to animate between the old and new target values
    are smoothly spliced together to create a smooth movement to the new
    target value that maintains the current velocity.

    The follow example shows one \l Rectangle tracking the position of another
    using SmoothedAnimation. The green rectangle's \c x and \c y values are
    bound to those of the red rectangle. Whenever these values change, the
    green rectangle smoothly animates to its new position:

    \snippet qml/smoothedanimation.qml 0

    A SmoothedAnimation can be configured by setting the \l velocity at which the
    animation should occur, or the \l duration that the animation should take.
    If both the \l velocity and \l duration are specified, the one that results in
    the quickest animation is chosen for each change in the target value.

    For example, animating from 0 to 800 will take 4 seconds if a velocity
    of 200 is set, will take 8 seconds with a duration of 8000 set, and will
    take 4 seconds with both a velocity of 200 and a duration of 8000 set.
    Animating from 0 to 20000 will take 10 seconds if a velocity of 200 is set,
    will take 8 seconds with a duration of 8000 set, and will take 8 seconds
    with both a velocity of 200 and a duration of 8000 set.

    The default velocity of SmoothedAnimation is 200 units/second.  Note that if the range of the
    value being animated is small, then the velocity will need to be adjusted
    appropriately.  For example, the opacity of an item ranges from 0 - 1.0.
    To enable a smooth animation in this range the velocity will need to be
    set to a value such as 0.5 units/second.  Animating from 0 to 1.0 with a velocity
    of 0.5 will take 2000 ms to complete.

    Like any other animation type, a SmoothedAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {Animation and Transitions in Qt Quick} documentation shows a
    variety of methods for creating animations.

    \sa SpringAnimation, NumberAnimation, {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation}
*/

QQuickSmoothedAnimation::QQuickSmoothedAnimation(QObject *parent)
: QQuickNumberAnimation(*(new QQuickSmoothedAnimationPrivate), parent)
{
}

QQuickSmoothedAnimation::~QQuickSmoothedAnimation()
{

}

QQuickSmoothedAnimationPrivate::QQuickSmoothedAnimationPrivate()
    : anim(new QSmoothedAnimation)
{
}

QQuickSmoothedAnimationPrivate::~QQuickSmoothedAnimationPrivate()
{
    typedef QHash<QQmlProperty, QSmoothedAnimation* >::iterator ActiveAnimationsHashIt;

    delete anim;
    for (ActiveAnimationsHashIt it = activeAnimations.begin(), end = activeAnimations.end(); it != end; ++it)
        it.value()->clearTemplate();
}

void QQuickSmoothedAnimationPrivate::updateRunningAnimations()
{
    for (QSmoothedAnimation *ease : qAsConst(activeAnimations)) {
        ease->maximumEasingTime = anim->maximumEasingTime;
        ease->reversingMode = anim->reversingMode;
        ease->velocity = anim->velocity;
        ease->userDuration = anim->userDuration;
        ease->init();
    }
}

QAbstractAnimationJob* QQuickSmoothedAnimation::transition(QQuickStateActions &actions,
                                               QQmlProperties &modified,
                                               TransitionDirection direction,
                                               QObject *defaultTarget)
{
    Q_UNUSED(direction);
    Q_D(QQuickSmoothedAnimation);

    const QQuickStateActions dataActions = QQuickPropertyAnimation::createTransitionActions(actions, modified, defaultTarget);

    QContinuingAnimationGroupJob *wrapperGroup = new QContinuingAnimationGroupJob();

    if (!dataActions.isEmpty()) {
        QSet<QAbstractAnimationJob*> anims;
        for (int i = 0; i < dataActions.size(); i++) {
            QSmoothedAnimation *ease;
            bool isActive;
            if (!d->activeAnimations.contains(dataActions[i].property)) {
                ease = new QSmoothedAnimation(d);
                d->activeAnimations.insert(dataActions[i].property, ease);
                ease->target = dataActions[i].property;
                isActive = false;
            } else {
                ease = d->activeAnimations.value(dataActions[i].property);
                isActive = true;
            }
            wrapperGroup->appendAnimation(initInstance(ease));

            ease->to = dataActions[i].toValue.toReal();

            // copying public members from main value holder animation
            ease->maximumEasingTime = d->anim->maximumEasingTime;
            ease->reversingMode = d->anim->reversingMode;
            ease->velocity = d->anim->velocity;
            ease->userDuration = d->anim->userDuration;

            ease->initialVelocity = ease->trackVelocity;

            if (isActive)
                ease->prepareForRestart();
            anims.insert(ease);
        }

        const auto copy = d->activeAnimations;
        for (QSmoothedAnimation *ease : copy) {
            if (!anims.contains(ease)) {
                ease->clearTemplate();
                d->activeAnimations.remove(ease->target);
            }
        }
    }
    return wrapperGroup;
}

/*!
    \qmlproperty enumeration QtQuick::SmoothedAnimation::reversingMode

    Sets how the SmoothedAnimation behaves if an animation direction is reversed.

    Possible values are:

    \list
    \li SmoothedAnimation.Eased (default) - the animation will smoothly decelerate, and then reverse direction
    \li SmoothedAnimation.Immediate - the animation will immediately begin accelerating in the reverse direction, beginning with a velocity of 0
    \li SmoothedAnimation.Sync - the property is immediately set to the target value
    \endlist
*/
QQuickSmoothedAnimation::ReversingMode QQuickSmoothedAnimation::reversingMode() const
{
    Q_D(const QQuickSmoothedAnimation);
    return (QQuickSmoothedAnimation::ReversingMode) d->anim->reversingMode;
}

void QQuickSmoothedAnimation::setReversingMode(ReversingMode m)
{
    Q_D(QQuickSmoothedAnimation);
    if (d->anim->reversingMode == m)
        return;

    d->anim->reversingMode = m;
    emit reversingModeChanged();
    d->updateRunningAnimations();
}

/*!
    \qmlproperty int QtQuick::SmoothedAnimation::duration

    This property holds the animation duration, in msecs, used when tracking the source.

    Setting this to -1 (the default) disables the duration value.

    If the velocity value and the duration value are both enabled, then the animation will
    use whichever gives the shorter duration.
*/
int QQuickSmoothedAnimation::duration() const
{
    Q_D(const QQuickSmoothedAnimation);
    return d->anim->userDuration;
}

void QQuickSmoothedAnimation::setDuration(int duration)
{
    Q_D(QQuickSmoothedAnimation);
    if (duration != -1)
        QQuickNumberAnimation::setDuration(duration);
    if(duration == d->anim->userDuration)
        return;
    d->anim->userDuration = duration;
    d->updateRunningAnimations();
}

qreal QQuickSmoothedAnimation::velocity() const
{
    Q_D(const QQuickSmoothedAnimation);
    return d->anim->velocity;
}

/*!
    \qmlproperty real QtQuick::SmoothedAnimation::velocity

    This property holds the average velocity allowed when tracking the 'to' value.

    The default velocity of SmoothedAnimation is 200 units/second.

    Setting this to -1 disables the velocity value.

    If the velocity value and the duration value are both enabled, then the animation will
    use whichever gives the shorter duration.
*/
void QQuickSmoothedAnimation::setVelocity(qreal v)
{
    Q_D(QQuickSmoothedAnimation);
    if (d->anim->velocity == v)
        return;

    d->anim->velocity = v;
    emit velocityChanged();
    d->updateRunningAnimations();
}

/*!
    \qmlproperty int QtQuick::SmoothedAnimation::maximumEasingTime

    This property specifies the maximum time, in msecs, any "eases" during the follow should take.
    Setting this property causes the velocity to "level out" after at a time.  Setting
    a negative value reverts to the normal mode of easing over the entire animation
    duration.

    The default value is -1.
*/
int QQuickSmoothedAnimation::maximumEasingTime() const
{
    Q_D(const QQuickSmoothedAnimation);
    return d->anim->maximumEasingTime;
}

void QQuickSmoothedAnimation::setMaximumEasingTime(int v)
{
    Q_D(QQuickSmoothedAnimation);
    if(v == d->anim->maximumEasingTime)
        return;
    d->anim->maximumEasingTime = v;
    emit maximumEasingTimeChanged();
    d->updateRunningAnimations();
}

QT_END_NAMESPACE
