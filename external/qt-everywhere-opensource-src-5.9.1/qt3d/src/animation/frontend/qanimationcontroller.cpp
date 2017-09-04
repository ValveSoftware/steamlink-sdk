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

#include "qanimationcontroller.h"
#include "qanimationgroup.h"

#include <private/qanimationcontroller_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

/*!
    \class Qt3DAnimation::QAnimationController
    \brief A controller class for animations
    \inmodule Qt3DAnimation
    \since 5.9
    \inherits QObject

    Qt3DAnimation::QAnimationController class controls the selection and playback of animations.
    The class can be used to find all animations from Qt3DCore::QEntity tree and create
    \l {Qt3DAnimation::QAnimationGroup} {QAnimationGroups} from the animations with the same name.
    The user can select which animation group is currently controlled with the animation
    controller by setting the active animation. The animation position is then propagated to
    that group after scaling and offsetting the provided position value with the
    positionScale and positionOffset values.

    \note that the animation controller doesn't have internal timer, but instead the user
    is responsible for updating the position property in timely manner.
*/

/*!
    \qmltype AnimationController
    \brief A controller type for animations
    \inqmlmodule Qt3D.Animation
    \since 5.9
    \instantiates Qt3DAnimation::QAnimationController

    AnimationController type controls the selection and playback of animations.
    The type can be used to find all animations from Entity tree and create
    \l {AnimationGroup} {AnimationGroups} from the animations with the same name.
    The user can select which animation group is currently controlled with the animation
    controller by setting the active animation. The animation position is then propagated to
    that group after scaling and offsetting the provided position value with the
    positionScale and positionOffset values.

    \note that the animation controller doesn't have internal timer, but instead the user
    is responsible for updating the position property in timely manner.
*/

/*!
    \property Qt3DAnimation::QAnimationController::activeAnimationGroup
    Holds the currectly active animation group.
*/
/*!
    \property Qt3DAnimation::QAnimationController::position
    Holds the current position of the animation. When the position is set,
    it is scaled and offset with positionScale/positionOffset and propagated
    to the active animation group.
*/
/*!
    \property Qt3DAnimation::QAnimationController::positionScale
    Holds the position scale of the animation.
*/
/*!
    \property Qt3DAnimation::QAnimationController::positionOffset
    Holds the position offset of the animation.
*/
/*!
    \property Qt3DAnimation::QAnimationController::entity
    Holds the entity animations are gathered and grouped from. If the controller already
    holds animations, they are cleared.
*/
/*!
    \property Qt3DAnimation::QAnimationController::recursive
    Holds whether the recursively search the entity tree when gathering animations from the entity.
    If set to true, the animations are searched also from the child entities of the entity.
    If set to false, only the entity passed to the controller is searched.
*/

/*!
    \qmlproperty int AnimationController::activeAnimationGroup
    Holds the currectly active animation group.
*/
/*!
    \qmlproperty real AnimationController::position
    Holds the current position of the animation. When the position is set,
    it is scaled and offset with positionScale/positionOffset and propagated
    to the active animation group.
*/
/*!
    \qmlproperty real AnimationController::positionScale
    Holds the position scale of the animation.
*/
/*!
    \qmlproperty real AnimationController::positionOffset
    Holds the position offset of the animation.
*/
/*!
    \qmlproperty Entity AnimationController::entity
    Holds the entity animations are gathered and grouped from. If the controller already
    holds animations, they are cleared.
*/
/*!
    \qmlproperty bool AnimationController::recursive
    Holds whether the recursively search the entity tree when gathering animations from the entity.
    If set to true, the animations are searched also from the child entities of the entity.
    If set to false, only the entity passed to the controller is searched.
*/
/*!
    \qmlproperty list<AnimationGroup> AnimationController::animationGroups
    Holds the list of animation groups in the controller.
*/
/*!
    \qmlmethod int Qt3D.Animation::AnimationController::getAnimationIndex(name)
    Returns the index of the animation with \a name. Returns -1 if no AnimationGroup
    with the given name is found.
*/
/*!
    \qmlmethod AnimationGroup Qt3D.Animation::AnimationController::getGroup(index)
    Returns the AnimationGroup with the given \a index.
*/

QAnimationControllerPrivate::QAnimationControllerPrivate()
    : QObjectPrivate()
    , m_activeAnimationGroup(0)
    , m_position(0.0f)
    , m_scaledPosition(0.0f)
    , m_positionScale(1.0f)
    , m_positionOffset(0.0f)
    , m_entity(nullptr)
    , m_recursive(true)
{

}

void QAnimationControllerPrivate::updatePosition(float position)
{
    m_position = position;
    m_scaledPosition = scaledPosition(position);
    if (m_activeAnimationGroup >= 0 && m_activeAnimationGroup < m_animationGroups.size())
        m_animationGroups[m_activeAnimationGroup]->setPosition(m_scaledPosition);
}

float QAnimationControllerPrivate::scaledPosition(float position) const
{
    return m_positionScale * position + m_positionOffset;
}

QAnimationGroup *QAnimationControllerPrivate::findGroup(const QString &name)
{
    for (QAnimationGroup *g : qAsConst(m_animationGroups)) {
        if (g->name() == name)
            return g;
    }
    return nullptr;
}

void QAnimationControllerPrivate::extractAnimations()
{
    Q_Q(QAnimationController);
    if (!m_entity)
        return;
    QList<Qt3DAnimation::QAbstractAnimation *> animations
            = m_entity->findChildren<Qt3DAnimation::QAbstractAnimation *>(QString(),
                m_recursive ? Qt::FindChildrenRecursively : Qt::FindDirectChildrenOnly);
    if (animations.size() > 0) {
        for (Qt3DAnimation::QAbstractAnimation *a : animations) {
            QAnimationGroup *group = findGroup(a->animationName());
            if (!group) {
                group = new QAnimationGroup(q);
                group->setName(a->animationName());
                m_animationGroups.push_back(group);
            }
            group->addAnimation(a);
        }
    }
}
void QAnimationControllerPrivate::clearAnimations()
{
    for (Qt3DAnimation::QAnimationGroup *a : qAsConst(m_animationGroups))
        a->deleteLater();
    m_animationGroups.clear();
    m_activeAnimationGroup = 0;
}

/*!
    Constructs a new QAnimationController with \a parent.
 */
QAnimationController::QAnimationController(QObject *parent)
    : QObject(*new QAnimationControllerPrivate, parent)
{

}

/*!
    Returns the list of animation groups the conroller is currently holding.
 */
QVector<QAnimationGroup *> QAnimationController::animationGroupList()
{
    Q_D(QAnimationController);
    return d->m_animationGroups;
}

int QAnimationController::activeAnimationGroup() const
{
    Q_D(const QAnimationController);
    return d->m_activeAnimationGroup;
}

float QAnimationController::position() const
{
    Q_D(const QAnimationController);
    return d->m_position;
}

float QAnimationController::positionScale() const
{
    Q_D(const QAnimationController);
    return d->m_positionScale;
}

float QAnimationController::positionOffset() const
{
    Q_D(const QAnimationController);
    return d->m_positionOffset;
}

Qt3DCore::QEntity *QAnimationController::entity() const
{
    Q_D(const QAnimationController);
    return d->m_entity;
}

bool QAnimationController::recursive() const
{
    Q_D(const QAnimationController);
    return d->m_recursive;
}

/*!
    Sets the \a animationGroups for the controller. Old groups are cleared.
 */
void QAnimationController::setAnimationGroups(const QVector<Qt3DAnimation::QAnimationGroup *> &animationGroups)
{
    Q_D(QAnimationController);
    d->m_animationGroups = animationGroups;
    if (d->m_activeAnimationGroup >= d->m_animationGroups.size())
        d->m_activeAnimationGroup = 0;
    d->updatePosition(d->m_position);
}

/*!
    Adds the given \a animationGroup to the controller.
 */
void QAnimationController::addAnimationGroup(Qt3DAnimation::QAnimationGroup *animationGroup)
{
    Q_D(QAnimationController);
    if (!d->m_animationGroups.contains(animationGroup))
        d->m_animationGroups.push_back(animationGroup);
}

/*!
    Removes the given \a animationGroup from the controller.
 */
void QAnimationController::removeAnimationGroup(Qt3DAnimation::QAnimationGroup *animationGroup)
{
    Q_D(QAnimationController);
    if (d->m_animationGroups.contains(animationGroup))
        d->m_animationGroups.removeAll(animationGroup);
    if (d->m_activeAnimationGroup >= d->m_animationGroups.size())
        d->m_activeAnimationGroup = 0;
}

void QAnimationController::setActiveAnimationGroup(int index)
{
    Q_D(QAnimationController);
    if (d->m_activeAnimationGroup != index) {
        d->m_activeAnimationGroup = index;
        d->updatePosition(d->m_position);
        emit activeAnimationGroupChanged(index);
    }
}
void QAnimationController::setPosition(float position)
{
    Q_D(QAnimationController);
    if (!qFuzzyCompare(d->m_scaledPosition, d->scaledPosition(position))) {
        d->updatePosition(position);
        emit positionChanged(position);
    }
}

void QAnimationController::setPositionScale(float scale)
{
    Q_D(QAnimationController);
    if (!qFuzzyCompare(d->m_positionScale, scale)) {
        d->m_positionScale = scale;
        emit positionScaleChanged(scale);
    }
}

void QAnimationController::setPositionOffset(float offset)
{
    Q_D(QAnimationController);
    if (!qFuzzyCompare(d->m_positionOffset, offset)) {
        d->m_positionOffset = offset;
        emit positionOffsetChanged(offset);
    }
}

void QAnimationController::setEntity(Qt3DCore::QEntity *entity)
{
    Q_D(QAnimationController);
    if (d->m_entity != entity) {
        d->clearAnimations();
        d->m_entity = entity;
        d->extractAnimations();
        d->updatePosition(d->m_position);
        emit entityChanged(entity);
    }
}

void QAnimationController::setRecursive(bool recursive)
{
    Q_D(QAnimationController);
    if (d->m_recursive != recursive) {
        d->m_recursive = recursive;
        emit recursiveChanged(recursive);
    }
}

/*!
    Returns the index of the animation with \a name. Returns -1 if no AnimationGroup
    with the given name is found.
*/
int QAnimationController::getAnimationIndex(const QString &name) const
{
    Q_D(const QAnimationController);
    for (int i = 0; i < d->m_animationGroups.size(); ++i) {
        if (d->m_animationGroups[i]->name() == name)
            return i;
    }
    return -1;
}

/*!
    Returns the AnimationGroup with the given \a index.
*/
QAnimationGroup *QAnimationController::getGroup(int index) const
{
    Q_D(const QAnimationController);
    return d->m_animationGroups.at(index);
}

} // Qt3DAnimation

QT_END_NAMESPACE
