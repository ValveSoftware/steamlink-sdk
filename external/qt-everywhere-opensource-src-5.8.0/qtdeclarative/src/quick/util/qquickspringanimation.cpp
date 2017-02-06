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

#include "qquickspringanimation_p.h"

#include "qquickanimation_p_p.h"
#include <private/qqmlproperty_p.h>
#include "private/qcontinuinganimationgroupjob_p.h"

#include <QtCore/qdebug.h>

#include <private/qobject_p.h>

#include <cmath>

#define DELAY_STOP_TIMER_INTERVAL 32

QT_BEGIN_NAMESPACE

class QQuickSpringAnimationPrivate;
class Q_AUTOTEST_EXPORT QSpringAnimation : public QAbstractAnimationJob
{
    Q_DISABLE_COPY(QSpringAnimation)
public:
    QSpringAnimation(QQuickSpringAnimationPrivate * = 0);

    ~QSpringAnimation();
    int duration() const;
    void restart();
    void init();

    qreal currentValue;
    qreal to;
    qreal velocity;
    int startTime;
    int dura;
    int lastTime;
    int stopTime;
    enum Mode {
        Track,
        Velocity,
        Spring
    };
    Mode mode;
    QQmlProperty target;

    qreal velocityms;
    qreal maxVelocity;
    qreal mass;
    qreal spring;
    qreal damping;
    qreal epsilon;
    qreal modulus;

    bool useMass : 1;
    bool haveModulus : 1;
    bool skipUpdate : 1;
    typedef QHash<QQmlProperty, QSpringAnimation*> ActiveAnimationHash;
    typedef ActiveAnimationHash::Iterator ActiveAnimationHashIt;

    void clearTemplate() { animationTemplate = 0; }

protected:
    virtual void updateCurrentTime(int time);
    virtual void updateState(QAbstractAnimationJob::State, QAbstractAnimationJob::State);
    void debugAnimation(QDebug d) const;

private:
    QQuickSpringAnimationPrivate *animationTemplate;
};

class QQuickSpringAnimationPrivate : public QQuickPropertyAnimationPrivate
{
    Q_DECLARE_PUBLIC(QQuickSpringAnimation)
public:
    QQuickSpringAnimationPrivate()
    : QQuickPropertyAnimationPrivate()
    , velocityms(0)
    , maxVelocity(0)
    , mass(1.0)
    , spring(0.)
    , damping(0.)
    , epsilon(0.01)
    , modulus(0.0)
    , useMass(false)
    , haveModulus(false)
    , mode(QSpringAnimation::Track)
    { elapsed.start(); }

    void updateMode();
    qreal velocityms;
    qreal maxVelocity;
    qreal mass;
    qreal spring;
    qreal damping;
    qreal epsilon;
    qreal modulus;

    bool useMass : 1;
    bool haveModulus : 1;
    QSpringAnimation::Mode mode;

    QSpringAnimation::ActiveAnimationHash activeAnimations;
    QElapsedTimer elapsed;
};

QSpringAnimation::QSpringAnimation(QQuickSpringAnimationPrivate *priv)
    : QAbstractAnimationJob()
    , currentValue(0)
    , to(0)
    , velocity(0)
    , startTime(0)
    , dura(0)
    , lastTime(0)
    , stopTime(-1)
    , mode(Track)
    , velocityms(0)
    , maxVelocity(0)
    , mass(1.0)
    , spring(0.)
    , damping(0.)
    , epsilon(0.01)
    , modulus(0.0)
    , useMass(false)
    , haveModulus(false)
    , skipUpdate(false)
    , animationTemplate(priv)
{
}

QSpringAnimation::~QSpringAnimation()
{
    if (animationTemplate) {
        if (target.object()) {
            ActiveAnimationHashIt it = animationTemplate->activeAnimations.find(target);
            if (it != animationTemplate->activeAnimations.end() && it.value() == this)
                animationTemplate->activeAnimations.erase(it);
        } else {
            //target is no longer valid, need to search linearly
            for (ActiveAnimationHashIt it = animationTemplate->activeAnimations.begin(); it != animationTemplate->activeAnimations.end(); ++it) {
                if (it.value() == this) {
                    animationTemplate->activeAnimations.erase(it);
                    break;
                }
            }
        }
    }
}

int QSpringAnimation::duration() const
{
    return -1;
}

void QSpringAnimation::restart()
{
    if (isRunning() || (stopTime != -1 && (animationTemplate->elapsed.elapsed() - stopTime) < DELAY_STOP_TIMER_INTERVAL)) {
        skipUpdate = true;
        init();
    } else {
        skipUpdate = false;
        //init() will be triggered when group starts
    }
}

void QSpringAnimation::init()
{
    lastTime = startTime = 0;
    stopTime = -1;
}

void QSpringAnimation::updateCurrentTime(int time)
{
    if (skipUpdate) {
        skipUpdate = false;
        return;
    }

    if (mode == Track) {
        stop();
        return;
    }

    int elapsed = time - lastTime;

    if (!elapsed)
        return;

    int count = elapsed / 16;

    if (mode == Spring) {
        if (elapsed < 16) // capped at 62fps.
            return;
        lastTime = time - (elapsed - count * 16);
    } else {
        lastTime = time;
    }

    qreal srcVal = to;

    bool stopped = false;

    if (haveModulus) {
        currentValue = fmod(currentValue, modulus);
        srcVal = fmod(srcVal, modulus);
    }
    if (mode == Spring) {
        // Real men solve the spring DEs using RK4.
        // We'll do something much simpler which gives a result that looks fine.
        for (int i = 0; i < count; ++i) {
            qreal diff = srcVal - currentValue;
            if (haveModulus && qAbs(diff) > modulus / 2) {
                if (diff < 0)
                    diff += modulus;
                else
                    diff -= modulus;
            }
            if (useMass)
                velocity = velocity + (spring * diff - damping * velocity) / mass;
            else
                velocity = velocity + spring * diff - damping * velocity;
            if (maxVelocity > 0.) {
                // limit velocity
                if (velocity > maxVelocity)
                    velocity = maxVelocity;
                else if (velocity < -maxVelocity)
                    velocity = -maxVelocity;
            }
            currentValue += velocity * 16.0 / 1000.0;
            if (haveModulus) {
                currentValue = fmod(currentValue, modulus);
                if (currentValue < 0.0)
                    currentValue += modulus;
            }
        }
        if (qAbs(velocity) < epsilon && qAbs(srcVal - currentValue) < epsilon) {
            velocity = 0.0;
            currentValue = srcVal;
            stopped = true;
        }
    } else {
        qreal moveBy = elapsed * velocityms;
        qreal diff = srcVal - currentValue;
        if (haveModulus && qAbs(diff) > modulus / 2) {
            if (diff < 0)
                diff += modulus;
            else
                diff -= modulus;
        }
        if (diff > 0) {
            currentValue += moveBy;
            if (haveModulus)
                currentValue = std::fmod(currentValue, modulus);
        } else {
            currentValue -= moveBy;
            if (haveModulus && currentValue < 0.0)
                currentValue = std::fmod(currentValue, modulus) + modulus;
        }
        if (lastTime - startTime >= dura) {
            currentValue = to;
            stopped = true;
        }
    }

    qreal old_to = to;

    QQmlPropertyPrivate::write(target, currentValue,
                                       QQmlPropertyData::BypassInterceptor |
                                       QQmlPropertyData::DontRemoveBinding);

    if (stopped && old_to == to) { // do not stop if we got restarted
        if (animationTemplate)
            stopTime = animationTemplate->elapsed.elapsed();
        stop();
    }
}

void QSpringAnimation::updateState(QAbstractAnimationJob::State newState, QAbstractAnimationJob::State /*oldState*/)
{
    if (newState == QAbstractAnimationJob::Running)
        init();
}

void QSpringAnimation::debugAnimation(QDebug d) const
{
    d << "SpringAnimationJob(" << hex << (const void *) this << dec << ")" << "velocity:" << maxVelocity
      << "spring:" << spring << "damping:" << damping << "epsilon:" << epsilon << "modulus:" << modulus
      << "mass:" << mass << "target:" << target.object() << "property:" << target.name()
      << "to:" << to << "current velocity:" << velocity;
}


void QQuickSpringAnimationPrivate::updateMode()
{
    if (spring == 0. && maxVelocity == 0.)
        mode = QSpringAnimation::Track;
    else if (spring > 0.)
        mode = QSpringAnimation::Spring;
    else {
        mode = QSpringAnimation::Velocity;
        for (QSpringAnimation::ActiveAnimationHashIt it = activeAnimations.begin(), end = activeAnimations.end(); it != end; ++it) {
            QSpringAnimation *animation = *it;
            animation->startTime = animation->lastTime;
            qreal dist = qAbs(animation->currentValue - animation->to);
            if (haveModulus && dist > modulus / 2)
                dist = modulus - fmod(dist, modulus);
            animation->dura = dist / velocityms;
        }
    }
}

/*!
    \qmltype SpringAnimation
    \instantiates QQuickSpringAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-transitions-animations
    \inherits NumberAnimation

    \brief Allows a property to track a value in a spring-like motion

    SpringAnimation mimics the oscillatory behavior of a spring, with the appropriate \l spring constant to
    control the acceleration and the \l damping to control how quickly the effect dies away.

    You can also limit the maximum \l velocity of the animation.

    The following \l Rectangle moves to the position of the mouse using a
    SpringAnimation when the mouse is clicked. The use of the \l Behavior
    on the \c x and \c y values indicates that whenever these values are
    changed, a SpringAnimation should be applied.

    \snippet qml/springanimation.qml 0

    Like any other animation type, a SpringAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {Animation and Transitions in Qt Quick} documentation shows a
    variety of methods for creating animations.

    \sa SmoothedAnimation, {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation}, {Qt Quick Demo - Clocks}
*/

QQuickSpringAnimation::QQuickSpringAnimation(QObject *parent)
: QQuickNumberAnimation(*(new QQuickSpringAnimationPrivate),parent)
{
}

QQuickSpringAnimation::~QQuickSpringAnimation()
{
    Q_D(QQuickSpringAnimation);
    for (QSpringAnimation::ActiveAnimationHashIt it = d->activeAnimations.begin(), end = d->activeAnimations.end(); it != end; ++it)
        it.value()->clearTemplate();
}

/*!
    \qmlproperty real QtQuick::SpringAnimation::velocity

    This property holds the maximum velocity allowed when tracking the source.

    The default value is 0 (no maximum velocity).
*/

qreal QQuickSpringAnimation::velocity() const
{
    Q_D(const QQuickSpringAnimation);
    return d->maxVelocity;
}

void QQuickSpringAnimation::setVelocity(qreal velocity)
{
    Q_D(QQuickSpringAnimation);
    d->maxVelocity = velocity;
    d->velocityms = velocity / 1000.0;
    d->updateMode();
}

/*!
    \qmlproperty real QtQuick::SpringAnimation::spring

    This property describes how strongly the target is pulled towards the
    source. The default value is 0 (that is, the spring-like motion is disabled).

    The useful value range is 0 - 5.0.

    When this property is set and the \l velocity value is greater than 0,
    the \l velocity limits the maximum speed.
*/
qreal QQuickSpringAnimation::spring() const
{
    Q_D(const QQuickSpringAnimation);
    return d->spring;
}

void QQuickSpringAnimation::setSpring(qreal spring)
{
    Q_D(QQuickSpringAnimation);
    d->spring = spring;
    d->updateMode();
}

/*!
    \qmlproperty real QtQuick::SpringAnimation::damping
    This property holds the spring damping value.

    This value describes how quickly the spring-like motion comes to rest.
    The default value is 0.

    The useful value range is 0 - 1.0. The lower the value, the faster it
    comes to rest.
*/
qreal QQuickSpringAnimation::damping() const
{
    Q_D(const QQuickSpringAnimation);
    return d->damping;
}

void QQuickSpringAnimation::setDamping(qreal damping)
{
    Q_D(QQuickSpringAnimation);
    if (damping > 1.)
        damping = 1.;

    d->damping = damping;
}


/*!
    \qmlproperty real QtQuick::SpringAnimation::epsilon
    This property holds the spring epsilon.

    The epsilon is the rate and amount of change in the value which is close enough
    to 0 to be considered equal to zero. This will depend on the usage of the value.
    For pixel positions, 0.25 would suffice. For scale, 0.005 will suffice.

    The default is 0.01. Tuning this value can provide small performance improvements.
*/
qreal QQuickSpringAnimation::epsilon() const
{
    Q_D(const QQuickSpringAnimation);
    return d->epsilon;
}

void QQuickSpringAnimation::setEpsilon(qreal epsilon)
{
    Q_D(QQuickSpringAnimation);
    d->epsilon = epsilon;
}

/*!
    \qmlproperty real QtQuick::SpringAnimation::modulus
    This property holds the modulus value. The default value is 0.

    Setting a \a modulus forces the target value to "wrap around" at the modulus.
    For example, setting the modulus to 360 will cause a value of 370 to wrap around to 10.
*/
qreal QQuickSpringAnimation::modulus() const
{
    Q_D(const QQuickSpringAnimation);
    return d->modulus;
}

void QQuickSpringAnimation::setModulus(qreal modulus)
{
    Q_D(QQuickSpringAnimation);
    if (d->modulus != modulus) {
        d->haveModulus = modulus != 0.0;
        d->modulus = modulus;
        d->updateMode();
        emit modulusChanged();
    }
}

/*!
    \qmlproperty real QtQuick::SpringAnimation::mass
    This property holds the "mass" of the property being moved.

    The value is 1.0 by default.

    A greater mass causes slower movement and a greater spring-like
    motion when an item comes to rest.
*/
qreal QQuickSpringAnimation::mass() const
{
    Q_D(const QQuickSpringAnimation);
    return d->mass;
}

void QQuickSpringAnimation::setMass(qreal mass)
{
    Q_D(QQuickSpringAnimation);
    if (d->mass != mass && mass > 0.0) {
        d->useMass = mass != 1.0;
        d->mass = mass;
        emit massChanged();
    }
}

QAbstractAnimationJob* QQuickSpringAnimation::transition(QQuickStateActions &actions,
                                                                   QQmlProperties &modified,
                                                                   TransitionDirection direction,
                                                                   QObject *defaultTarget)
{
    Q_D(QQuickSpringAnimation);
    Q_UNUSED(direction);

    QContinuingAnimationGroupJob *wrapperGroup = new QContinuingAnimationGroupJob();

    QQuickStateActions dataActions = QQuickNumberAnimation::createTransitionActions(actions, modified, defaultTarget);
    if (!dataActions.isEmpty()) {
        QSet<QAbstractAnimationJob*> anims;
        for (int i = 0; i < dataActions.size(); ++i) {
            QSpringAnimation *animation;
            bool needsRestart = false;
            const QQmlProperty &property = dataActions.at(i).property;
            if (d->activeAnimations.contains(property)) {
                animation = d->activeAnimations[property];
                needsRestart = true;
            } else {
                animation = new QSpringAnimation(d);
                d->activeAnimations.insert(property, animation);
                animation->target = property;
            }
            wrapperGroup->appendAnimation(initInstance(animation));

            animation->to = dataActions.at(i).toValue.toReal();
            animation->startTime = 0;
            animation->velocityms = d->velocityms;
            animation->mass = d->mass;
            animation->spring = d->spring;
            animation->damping = d->damping;
            animation->epsilon = d->epsilon;
            animation->modulus = d->modulus;
            animation->useMass = d->useMass;
            animation->haveModulus = d->haveModulus;
            animation->mode = d->mode;
            animation->dura = -1;
            animation->maxVelocity = d->maxVelocity;

            if (d->fromIsDefined)
                animation->currentValue = dataActions.at(i).fromValue.toReal();
            else
                animation->currentValue = property.read().toReal();
            if (animation->mode == QSpringAnimation::Velocity) {
                qreal dist = qAbs(animation->currentValue - animation->to);
                if (d->haveModulus && dist > d->modulus / 2)
                    dist = d->modulus - fmod(dist, d->modulus);
                animation->dura = dist / animation->velocityms;
            }

            if (needsRestart)
                animation->restart();
            anims.insert(animation);
        }
        const auto copy = d->activeAnimations;
        for (QSpringAnimation *anim : copy) {
            if (!anims.contains(anim)) {
                anim->clearTemplate();
                d->activeAnimations.remove(anim->target);
            }
        }
    }
    return wrapperGroup;
}

QT_END_NAMESPACE
