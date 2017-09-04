/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qkeyframeanimation.h"
#include "Qt3DAnimation/private/qkeyframeanimation_p.h"

#include <cmath>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

/*!
    \class Qt3DAnimation::QKeyframeAnimation
    \brief A class implementing simple keyframe animation to a QTransform
    \inmodule Qt3DAnimation
    \since 5.9
    \inherits Qt3DAnimation::QAbstractAnimation

    A Qt3DAnimation::QKeyframeAnimation class implements simple keyframe animation
    that can be used to animate \l QTransform. The keyframes consists of multiple
    timed QTransforms, which are interpolated and applied to the target \l QTransform.
    \l QEasingCurve is used between keyframes to control the interpolator. RepeatMode
    can be set for when the position set to the QKeyframeAnimation is below or above
    the values defined in the keyframe positions.
*/

/*!
    \qmltype KeyframeAnimation
    \brief A type implementing simple keyframe animation to a Transform
    \inqmlmodule Qt3D.Animation
    \since 5.9
    \inherits AbstractAnimation
    \instantiates Qt3DAnimation::QKeyframeAnimation

    A KeyframeAnimation type implements simple keyframe animation
    that can be used to animate \l Transform. The keyframes consists of multiple
    timed \l {Qt3D.Core::Transform}s, which are interpolated and applied
    to the target Transform. EasingCurve is used between keyframes to control
    the interpolator. RepeatMode can be set for when the position set to the
    KeyframeAnimation is less or or greater than the values defined in the keyframe positions.
*/

/*!
    \property Qt3DAnimation::QKeyframeAnimation::framePositions
    Holds the positions of the keyframes. Each position in the list specifies the position
    of the corresponding keyframe with the same index. The values must be in an ascending order.
    Values can be positive or negative and do not have any predefined unit.
*/
/*!
    \property Qt3DAnimation::QKeyframeAnimation::target
    Holds the target QTransform the animation is applied to.
*/
/*!
    \property Qt3DAnimation::QKeyframeAnimation::easing
    Holds the easing curve of the interpolator between keyframes.
*/
/*!
    \property Qt3DAnimation::QKeyframeAnimation::targetName
    Holds the name of the target transform. This is a convenience property making it
    easier to match the target transform to the keyframe animation. The name
    is usually same as the name of the parent entity of the target transform, but
    does not have to be.
*/
/*!
    \property Qt3DAnimation::QKeyframeAnimation::startMode
    Holds the repeat mode for the position values less than the first frame position.
*/
/*!
    \property Qt3DAnimation::QKeyframeAnimation::endMode
    Holds the repeat mode for the position values greater than the last frame position.
*/
/*!
    \enum QKeyframeAnimation::RepeatMode

    This enumeration specifies how position values outside keyframe values are handled.
    \value None The animation is not applied to the target transform.
    \value Constant The edge keyframe value is used.
    \value Repeat The animation is repeated.
*/
/*!
    \qmlproperty list<real> KeyframeAnimation::framePositions
    Holds the positions of the keyframes. Each position in the list specifies the position
    of the corresponding keyframe. The values must be in an ascending order. Values can
    be positive or negative and do not have any predefined unit.
*/
/*!
    \qmlproperty Transform KeyframeAnimation::target
    Holds the target Transform the animation is applied to.
*/
/*!
    \qmlproperty EasingCurve KeyframeAnimation::easing
    Holds the easing curve of the interpolator between keyframes.
*/
/*!
    \qmlproperty string KeyframeAnimation::targetName
    Holds the name of the target transform. This is a convenience property making it
    easier to match the target transform to the keyframe animation. The name
    is usually same as the name of the parent entity of the target transform, but
    does not have to be.
*/
/*!
    \qmlproperty enumeration KeyframeAnimation::startMode
    Holds the repeat mode for the position values less than the first frame position.
    \list
    \li None
    \li Constant
    \li Repeat
    \endlist
*/
/*!
    \qmlproperty enumeration KeyframeAnimation::endMode
    Holds the repeat mode for the position values greater than the last frame position.
    \list
    \li None
    \li Constant
    \li Repeat
    \endlist
*/
/*!
    \qmlproperty list<Transform> KeyframeAnimation::keyframes
    Holds the list of keyframes in the keyframe animation.
*/

QKeyframeAnimationPrivate::QKeyframeAnimationPrivate()
    : QAbstractAnimationPrivate(QAbstractAnimation::KeyframeAnimation)
    , m_target(nullptr)
    , m_minposition(0.0f)
    , m_maxposition(0.0f)
    , m_startMode(QKeyframeAnimation::Constant)
    , m_endMode(QKeyframeAnimation::Constant)
{

}

/*!
    Constructs an QKeyframeAnimation with \a parent.
*/
QKeyframeAnimation::QKeyframeAnimation(QObject *parent)
    : QAbstractAnimation(*new QKeyframeAnimationPrivate(), parent)
{
    Q_D(QKeyframeAnimation);
    d->m_positionConnection = QObject::connect(this, &QAbstractAnimation::positionChanged,
                                               this, &QKeyframeAnimation::updateAnimation);
}


void QKeyframeAnimation::setFramePositions(const QVector<float> &positions)
{
    Q_D(QKeyframeAnimation);
    d->m_framePositions = positions;
    d->m_position = -1.0f;
    if (d->m_framePositions.size() == 0) {
        d->m_minposition = d->m_maxposition = 0.0f;
        return;
    }
    d->m_minposition = d->m_framePositions.first();
    d->m_maxposition = d->m_framePositions.last();
    float lastPos = d->m_minposition;
    for (float p : qAsConst(d->m_framePositions)) {
        if (p < lastPos || p > d->m_maxposition)
            qWarning() << "positions not ordered correctly";
        lastPos = p;
    }
    setDuration(d->m_maxposition);
}

/*!
    Sets the \a keyframes of the animation. Old keyframes are cleared.
 */
void QKeyframeAnimation::setKeyframes(const QVector<Qt3DCore::QTransform *> &keyframes)
{
    Q_D(QKeyframeAnimation);
    d->m_keyframes = keyframes;
}

// slerp which allows long path
QQuaternion lslerp(QQuaternion q1, QQuaternion q2, float t)
{
    QQuaternion ret;
    // Handle the easy cases first.
    if (t <= 0.0f)
        return q1;
    else if (t >= 1.0f)
        return q2;

    float cos = qBound(-1.0f, QQuaternion::dotProduct(q1, q2), 1.0f);
    float angle = std::acos(cos);
    float sin = std::sin(angle);
    if (!qFuzzyIsNull(sin)) {
        float a = std::sin((1.0 - t) * angle) / sin;
        float b = std::sin(t * angle) / sin;
        ret = (q1 * a + q2 * b).normalized();
    } else {
        ret = q1 * (1.0f-t) + q2 * t;
    }
    return ret;
}

void QKeyframeAnimationPrivate::calculateFrame(float position)
{
    if (m_target && m_framePositions.size() > 0
        && m_keyframes.size() == m_framePositions.size()) {
        if (position < m_minposition) {
            if (m_startMode == QKeyframeAnimation::None) {
                return;
            } else if (m_startMode == QKeyframeAnimation::Constant) {
                m_target->setRotation(m_keyframes.first()->rotation());
                m_target->setScale3D(m_keyframes.first()->scale3D());
                m_target->setTranslation(m_keyframes.first()->translation());
                return;
            } else {
                // must be repeat
                position = std::fmod(-(position - m_minposition), m_maxposition - m_minposition)
                                     + m_minposition;
            }
        } else if (position >= m_maxposition) {
            if (m_endMode == QKeyframeAnimation::None) {
                return;
            } else if (m_endMode == QKeyframeAnimation::Constant) {
                m_target->setRotation(m_keyframes.last()->rotation());
                m_target->setScale3D(m_keyframes.last()->scale3D());
                m_target->setTranslation(m_keyframes.last()->translation());
                return;
            } else {
                // must be repeat
                position = std::fmod(position - m_minposition, m_maxposition - m_minposition)
                                     + m_minposition;
            }
        }
        if (position >= m_minposition && position < m_maxposition) {
            for (int i = 0; i < m_framePositions.size() - 1; i++) {
                if (position >= m_framePositions.at(i)
                    && position < m_framePositions.at(i+1)) {
                    float ip = (position - m_framePositions.at(i))
                                / (m_framePositions.at(i+1) - m_framePositions.at(i));
                    float eIp = m_easing.valueForProgress(ip);
                    float eIip = 1.0f - eIp;

                    Qt3DCore::QTransform *a = m_keyframes.at(i);
                    Qt3DCore::QTransform *b = m_keyframes.at(i+1);

                    QVector3D s = a->scale3D() * eIip + b->scale3D() * eIp;
                    QVector3D t = a->translation() * eIip + b->translation() * eIp;
                    QQuaternion r = QQuaternion::slerp(a->rotation(), b->rotation(), eIp);

                    m_target->setRotation(r);
                    m_target->setScale3D(s);
                    m_target->setTranslation(t);
                    return;
                }
            }
        }
    }
}

void QKeyframeAnimation::updateAnimation(float position)
{
    Q_D(QKeyframeAnimation);
    d->calculateFrame(position);
}

QVector<float> QKeyframeAnimation::framePositions() const
{
    Q_D(const QKeyframeAnimation);
    return d->m_framePositions;
}

/*!
    Returns the list of keyframes.
 */
QVector<Qt3DCore::QTransform *> QKeyframeAnimation::keyframeList() const
{
    Q_D(const QKeyframeAnimation);
    return d->m_keyframes;
}

void QKeyframeAnimation::setTarget(Qt3DCore::QTransform *target)
{
    Q_D(QKeyframeAnimation);
    if (d->m_target != target) {
        d->m_target = target;
        emit targetChanged(d->m_target);
        d->m_position = -1.0f;

        if (target) {
            d->m_baseScale = target->scale3D();
            d->m_baseTranslation = target->translation();
            d->m_baseRotation = target->rotation();
        }
    }
}

QKeyframeAnimation::RepeatMode QKeyframeAnimation::startMode() const
{
    Q_D(const QKeyframeAnimation);
    return d->m_startMode;
}

QKeyframeAnimation::RepeatMode QKeyframeAnimation::endMode() const
{
    Q_D(const QKeyframeAnimation);
    return d->m_endMode;
}

void QKeyframeAnimation::setEasing(const QEasingCurve &easing)
{
    Q_D(QKeyframeAnimation);
    if (d->m_easing != easing) {
        d->m_easing = easing;
        emit easingChanged(easing);
    }
}

void QKeyframeAnimation::setTargetName(const QString &name)
{
    Q_D(QKeyframeAnimation);
    if (d->m_targetName != name) {
        d->m_targetName = name;
        emit targetNameChanged(name);
    }
}

void QKeyframeAnimation::setStartMode(QKeyframeAnimation::RepeatMode mode)
{
    Q_D(QKeyframeAnimation);
    if (d->m_startMode != mode) {
        d->m_startMode = mode;
        emit startModeChanged(mode);
    }
}

void QKeyframeAnimation::setEndMode(QKeyframeAnimation::RepeatMode mode)
{
    Q_D(QKeyframeAnimation);
    if (mode != d->m_endMode) {
        d->m_endMode = mode;
        emit endModeChanged(mode);
    }
}

/*!
    Adds new \a keyframe at the end of the animation. The QTransform can
    be added to the animation multiple times.
 */
void QKeyframeAnimation::addKeyframe(Qt3DCore::QTransform *keyframe)
{
    Q_D(QKeyframeAnimation);
    d->m_keyframes.push_back(keyframe);
}

/*!
    Removes a \a keyframe from the animation. If the same QTransform
    is set as keyframe multiple times, all occurrences are removed.
 */
void QKeyframeAnimation::removeKeyframe(Qt3DCore::QTransform *keyframe)
{
    Q_D(QKeyframeAnimation);
    d->m_keyframes.removeAll(keyframe);
}

QString QKeyframeAnimation::targetName() const
{
    Q_D(const QKeyframeAnimation);
    return d->m_targetName;
}

QEasingCurve QKeyframeAnimation::easing() const
{
    Q_D(const QKeyframeAnimation);
    return d->m_easing;
}

Qt3DCore::QTransform *QKeyframeAnimation::target() const
{
    Q_D(const QKeyframeAnimation);
    return d->m_target;
}

} // Qt3DAnimation

QT_END_NAMESPACE
