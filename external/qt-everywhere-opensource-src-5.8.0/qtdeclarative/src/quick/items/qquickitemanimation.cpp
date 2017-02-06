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

#include "qquickitemanimation_p.h"
#include "qquickitemanimation_p_p.h"
#include "qquickstateoperations_p.h"

#include <private/qqmlproperty_p.h>
#if QT_CONFIG(quick_path)
#include <private/qquickpath_p.h>
#endif
#include "private/qparallelanimationgroupjob_p.h"
#include "private/qsequentialanimationgroupjob_p.h"

#include <QtCore/qmath.h>
#include <QtGui/qtransform.h>
#include <QtQml/qqmlinfo.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype ParentAnimation
    \instantiates QQuickParentAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-properties
    \since 5.0
    \inherits Animation
    \brief Animates changes in parent values

    ParentAnimation is used to animate a parent change for an \l Item.

    For example, the following ParentChange changes \c blueRect to become
    a child of \c redRect when it is clicked. The inclusion of the
    ParentAnimation, which defines a NumberAnimation to be applied during
    the transition, ensures the item animates smoothly as it moves to
    its new parent:

    \snippet qml/parentanimation.qml 0

    A ParentAnimation can contain any number of animations. These animations will
    be run in parallel; to run them sequentially, define them within a
    SequentialAnimation.

    In some cases, such as when reparenting between items with clipping enabled, it is useful
    to animate the parent change via another item that does not have clipping
    enabled. Such an item can be set using the \l via property.

    ParentAnimation is typically used within a \l Transition in conjunction
    with a ParentChange. When used in this manner, it animates any
    ParentChange that has occurred during the state change. This can be
    overridden by setting a specific target item using the \l target property.

    \sa {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation}
*/
QQuickParentAnimation::QQuickParentAnimation(QObject *parent)
    : QQuickAnimationGroup(*(new QQuickParentAnimationPrivate), parent)
{
}

QQuickParentAnimation::~QQuickParentAnimation()
{
}

/*!
    \qmlproperty Item QtQuick::ParentAnimation::target
    The item to reparent.

    When used in a transition, if no target is specified, all
    ParentChange occurrences are animated by the ParentAnimation.
*/
QQuickItem *QQuickParentAnimation::target() const
{
    Q_D(const QQuickParentAnimation);
    return d->target;
}

void QQuickParentAnimation::setTargetObject(QQuickItem *target)
{
    Q_D(QQuickParentAnimation);
    if (target == d->target)
        return;

    d->target = target;
    emit targetChanged();
}

/*!
    \qmlproperty Item QtQuick::ParentAnimation::newParent
    The new parent to animate to.

    If the ParentAnimation is defined within a \l Transition,
    this value defaults to the value defined in the end state of the
    \l Transition.
*/
QQuickItem *QQuickParentAnimation::newParent() const
{
    Q_D(const QQuickParentAnimation);
    return d->newParent;
}

void QQuickParentAnimation::setNewParent(QQuickItem *newParent)
{
    Q_D(QQuickParentAnimation);
    if (newParent == d->newParent)
        return;

    d->newParent = newParent;
    emit newParentChanged();
}

/*!
    \qmlproperty Item QtQuick::ParentAnimation::via
    The item to reparent via. This provides a way to do an unclipped animation
    when both the old parent and new parent are clipped.

    \qml
    ParentAnimation {
        target: myItem
        via: topLevelItem
        // ...
    }
    \endqml

    \note This only works when the ParentAnimation is used in a \l Transition
    in conjunction with a ParentChange.
*/
QQuickItem *QQuickParentAnimation::via() const
{
    Q_D(const QQuickParentAnimation);
    return d->via;
}

void QQuickParentAnimation::setVia(QQuickItem *via)
{
    Q_D(QQuickParentAnimation);
    if (via == d->via)
        return;

    d->via = via;
    emit viaChanged();
}

//### mirrors same-named function in QQuickItem
QPointF QQuickParentAnimationPrivate::computeTransformOrigin(QQuickItem::TransformOrigin origin, qreal width, qreal height) const
{
    switch (origin) {
    default:
    case QQuickItem::TopLeft:
        return QPointF(0, 0);
    case QQuickItem::Top:
        return QPointF(width / 2., 0);
    case QQuickItem::TopRight:
        return QPointF(width, 0);
    case QQuickItem::Left:
        return QPointF(0, height / 2.);
    case QQuickItem::Center:
        return QPointF(width / 2., height / 2.);
    case QQuickItem::Right:
        return QPointF(width, height / 2.);
    case QQuickItem::BottomLeft:
        return QPointF(0, height);
    case QQuickItem::Bottom:
        return QPointF(width / 2., height);
    case QQuickItem::BottomRight:
        return QPointF(width, height);
    }
}

struct QQuickParentAnimationData : public QAbstractAnimationAction
{
    QQuickParentAnimationData() : reverse(false) {}
    ~QQuickParentAnimationData() { qDeleteAll(pc); }

    QQuickStateActions actions;
    //### reverse should probably apply on a per-action basis
    bool reverse;
    QList<QQuickParentChange *> pc;
    void doAction() Q_DECL_OVERRIDE
    {
        for (int ii = 0; ii < actions.count(); ++ii) {
            const QQuickStateAction &action = actions.at(ii);
            if (reverse)
                action.event->reverse();
            else
                action.event->execute();
        }
    }
};

QAbstractAnimationJob* QQuickParentAnimation::transition(QQuickStateActions &actions,
                        QQmlProperties &modified,
                        TransitionDirection direction,
                        QObject *defaultTarget)
{
    Q_D(QQuickParentAnimation);

    QQuickParentAnimationData *data = new QQuickParentAnimationData;
    QQuickParentAnimationData *viaData = new QQuickParentAnimationData;

    bool hasExplicit = false;
    if (d->target && d->newParent) {
        data->reverse = false;
        QQuickStateAction myAction;
        QQuickParentChange *pc = new QQuickParentChange;
        pc->setObject(d->target);
        pc->setParent(d->newParent);
        myAction.event = pc;
        data->pc << pc;
        data->actions << myAction;
        hasExplicit = true;
        if (d->via) {
            viaData->reverse = false;
            QQuickStateAction myVAction;
            QQuickParentChange *vpc = new QQuickParentChange;
            vpc->setObject(d->target);
            vpc->setParent(d->via);
            myVAction.event = vpc;
            viaData->pc << vpc;
            viaData->actions << myVAction;
        }
        //### once actions have concept of modified,
        //    loop to match appropriate ParentChanges and mark as modified
    }

    if (!hasExplicit)
    for (int i = 0; i < actions.size(); ++i) {
        QQuickStateAction &action = actions[i];
        if (action.event && action.event->type() == QQuickStateActionEvent::ParentChange
            && (!d->target || static_cast<QQuickParentChange*>(action.event)->object() == d->target)) {

            QQuickParentChange *pc = static_cast<QQuickParentChange*>(action.event);
            QQuickStateAction myAction = action;
            data->reverse = action.reverseEvent;

            //### this logic differs from PropertyAnimation
            //    (probably a result of modified vs. done)
            if (d->newParent) {
                QQuickParentChange *epc = new QQuickParentChange;
                epc->setObject(static_cast<QQuickParentChange*>(action.event)->object());
                epc->setParent(d->newParent);
                myAction.event = epc;
                data->pc << epc;
                data->actions << myAction;
                pc = epc;
            } else {
                action.actionDone = true;
                data->actions << myAction;
            }

            if (d->via) {
                viaData->reverse = false;
                QQuickStateAction myAction;
                QQuickParentChange *vpc = new QQuickParentChange;
                vpc->setObject(pc->object());
                vpc->setParent(d->via);
                myAction.event = vpc;
                viaData->pc << vpc;
                viaData->actions << myAction;
                QQuickStateAction dummyAction;
                QQuickStateAction &xAction = pc->xIsSet() && i < actions.size()-1 ? actions[++i] : dummyAction;
                QQuickStateAction &yAction = pc->yIsSet() && i < actions.size()-1 ? actions[++i] : dummyAction;
                QQuickStateAction &sAction = pc->scaleIsSet() && i < actions.size()-1 ? actions[++i] : dummyAction;
                QQuickStateAction &rAction = pc->rotationIsSet() && i < actions.size()-1 ? actions[++i] : dummyAction;
                QQuickItem *target = pc->object();
                QQuickItem *targetParent = action.reverseEvent ? pc->originalParent() : pc->parent();

                //### this mirrors the logic in QQuickParentChange.
                bool ok;
                const QTransform &transform = targetParent->itemTransform(d->via, &ok);
                if (transform.type() >= QTransform::TxShear || !ok) {
                    qmlInfo(this) << QQuickParentAnimation::tr("Unable to preserve appearance under complex transform");
                    ok = false;
                }

                qreal scale = 1;
                qreal rotation = 0;
                bool isRotate = (transform.type() == QTransform::TxRotate) || (transform.m11() < 0);
                if (ok && !isRotate) {
                    if (transform.m11() == transform.m22())
                        scale = transform.m11();
                    else {
                        qmlInfo(this) << QQuickParentAnimation::tr("Unable to preserve appearance under non-uniform scale");
                        ok = false;
                    }
                } else if (ok && isRotate) {
                    if (transform.m11() == transform.m22())
                        scale = qSqrt(transform.m11()*transform.m11() + transform.m12()*transform.m12());
                    else {
                        qmlInfo(this) << QQuickParentAnimation::tr("Unable to preserve appearance under non-uniform scale");
                        ok = false;
                    }

                    if (scale != 0)
                        rotation = qAtan2(transform.m12()/scale, transform.m11()/scale) * 180/M_PI;
                    else {
                        qmlInfo(this) << QQuickParentAnimation::tr("Unable to preserve appearance under scale of 0");
                        ok = false;
                    }
                }

                const QPointF &point = transform.map(QPointF(xAction.toValue.toReal(),yAction.toValue.toReal()));
                qreal x = point.x();
                qreal y = point.y();
                if (ok && target->transformOrigin() != QQuickItem::TopLeft) {
                    qreal w = target->width();
                    qreal h = target->height();
                    if (pc->widthIsSet() && i < actions.size() - 1)
                        w = actions.at(++i).toValue.toReal();
                    if (pc->heightIsSet() && i < actions.size() - 1)
                        h = actions.at(++i).toValue.toReal();
                    const QPointF &transformOrigin
                            = d->computeTransformOrigin(target->transformOrigin(), w,h);
                    qreal tempxt = transformOrigin.x();
                    qreal tempyt = transformOrigin.y();
                    QTransform t;
                    t.translate(-tempxt, -tempyt);
                    t.rotate(rotation);
                    t.scale(scale, scale);
                    t.translate(tempxt, tempyt);
                    const QPointF &offset = t.map(QPointF(0,0));
                    x += offset.x();
                    y += offset.y();
                }

                if (ok) {
                    //qDebug() << x << y << rotation << scale;
                    xAction.toValue = x;
                    yAction.toValue = y;
                    sAction.toValue = sAction.toValue.toReal() * scale;
                    rAction.toValue = rAction.toValue.toReal() + rotation;
                }
            }
        }
    }

    if (data->actions.count()) {
        QSequentialAnimationGroupJob *topLevelGroup = new QSequentialAnimationGroupJob;
        QActionAnimation *viaAction = d->via ? new QActionAnimation : 0;
        QActionAnimation *targetAction = new QActionAnimation;
        //we'll assume the common case by far is to have children, and always create ag
        QParallelAnimationGroupJob *ag = new QParallelAnimationGroupJob;

        if (d->via)
            viaAction->setAnimAction(viaData);
        targetAction->setAnimAction(data);

        //take care of any child animations
        bool valid = d->defaultProperty.isValid();
        QAbstractAnimationJob* anim;
        for (int ii = 0; ii < d->animations.count(); ++ii) {
            if (valid)
                d->animations.at(ii)->setDefaultTarget(d->defaultProperty);
            anim = d->animations.at(ii)->transition(actions, modified, direction, defaultTarget);
            if (anim)
                ag->appendAnimation(anim);
        }

        //TODO: simplify/clarify logic
        bool forwards = direction == QQuickAbstractAnimation::Forward;
        if (forwards) {
            topLevelGroup->appendAnimation(d->via ? viaAction : targetAction);
            topLevelGroup->appendAnimation(ag);
            if (d->via)
                topLevelGroup->appendAnimation(targetAction);
        } else {
            if (d->via)
                topLevelGroup->appendAnimation(targetAction);
            topLevelGroup->appendAnimation(ag);
            topLevelGroup->appendAnimation(d->via ? viaAction : targetAction);
        }
        return initInstance(topLevelGroup);
    } else {
        delete data;
        delete viaData;
    }
    return 0;
}

/*!
    \qmltype AnchorAnimation
    \instantiates QQuickAnchorAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-properties
    \inherits Animation
    \brief Animates changes in anchor values

    AnchorAnimation is used to animate an anchor change.

    In the following snippet we animate the addition of a right anchor to a \l Rectangle:

    \snippet qml/anchoranimation.qml 0

    When an AnchorAnimation is used in a \l Transition, it will
    animate any AnchorChanges that have occurred during the state change.
    This can be overridden by setting a specific target item using the
    \l {AnchorChanges::target}{AnchorChanges.target} property.

    \note AnchorAnimation can only be used in a \l Transition and in
    conjunction with an AnchorChange. It cannot be used in behaviors and
    other types of animations.

    \sa {Animation and Transitions in Qt Quick}, AnchorChanges
*/
QQuickAnchorAnimation::QQuickAnchorAnimation(QObject *parent)
: QQuickAbstractAnimation(*(new QQuickAnchorAnimationPrivate), parent)
{
}

QQuickAnchorAnimation::~QQuickAnchorAnimation()
{
}

/*!
    \qmlproperty list<Item> QtQuick::AnchorAnimation::targets
    The items to reanchor.

    If no targets are specified all AnchorChanges will be
    animated by the AnchorAnimation.
*/
QQmlListProperty<QQuickItem> QQuickAnchorAnimation::targets()
{
    Q_D(QQuickAnchorAnimation);
    return QQmlListProperty<QQuickItem>(this, d->targets);
}

/*!
    \qmlproperty int QtQuick::AnchorAnimation::duration
    This property holds the duration of the animation, in milliseconds.

    The default value is 250.
*/
int QQuickAnchorAnimation::duration() const
{
    Q_D(const QQuickAnchorAnimation);
    return d->duration;
}

void QQuickAnchorAnimation::setDuration(int duration)
{
    if (duration < 0) {
        qmlInfo(this) << tr("Cannot set a duration of < 0");
        return;
    }

    Q_D(QQuickAnchorAnimation);
    if (d->duration == duration)
        return;
    d->duration = duration;
    emit durationChanged(duration);
}

/*!
    \qmlpropertygroup QtQuick::AnchorAnimation::easing
    \qmlproperty enumeration QtQuick::AnchorAnimation::easing.type
    \qmlproperty real QtQuick::AnchorAnimation::easing.amplitude
    \qmlproperty real QtQuick::AnchorAnimation::easing.overshoot
    \qmlproperty real QtQuick::AnchorAnimation::easing.period
    \brief Specifies the easing curve used for the animation

    To specify an easing curve you need to specify at least the type. For some curves you can also specify
    amplitude, period and/or overshoot. The default easing curve is
    Linear.

    \qml
    AnchorAnimation { easing.type: Easing.InOutQuad }
    \endqml

    See the \l{PropertyAnimation::easing.type} documentation for information
    about the different types of easing curves.
*/
QEasingCurve QQuickAnchorAnimation::easing() const
{
    Q_D(const QQuickAnchorAnimation);
    return d->easing;
}

void QQuickAnchorAnimation::setEasing(const QEasingCurve &e)
{
    Q_D(QQuickAnchorAnimation);
    if (d->easing == e)
        return;

    d->easing = e;
    emit easingChanged(e);
}

QAbstractAnimationJob* QQuickAnchorAnimation::transition(QQuickStateActions &actions,
                        QQmlProperties &modified,
                        TransitionDirection direction,
                        QObject *defaultTarget)
{
    Q_UNUSED(modified);
    Q_UNUSED(defaultTarget);
    Q_D(QQuickAnchorAnimation);
    QQuickAnimationPropertyUpdater *data = new QQuickAnimationPropertyUpdater;
    data->interpolatorType = QMetaType::QReal;
    data->interpolator = d->interpolator;
    data->reverse = direction == Backward ? true : false;
    data->fromSourced = false;
    data->fromDefined = false;

    for (int ii = 0; ii < actions.count(); ++ii) {
        QQuickStateAction &action = actions[ii];
        if (action.event && action.event->type() == QQuickStateActionEvent::AnchorChanges
            && (d->targets.isEmpty() || d->targets.contains(static_cast<QQuickAnchorChanges*>(action.event)->object()))) {
            data->actions << static_cast<QQuickAnchorChanges*>(action.event)->additionalActions();
        }
    }

    QQuickBulkValueAnimator *animator = new QQuickBulkValueAnimator;
    if (data->actions.count()) {
        animator->setAnimValue(data);
        animator->setFromSourcedValue(&data->fromSourced);
    } else {
        delete data;
    }

    animator->setDuration(d->duration);
    animator->setEasingCurve(d->easing);
    return initInstance(animator);
}


#if QT_CONFIG(quick_path)
/*!
    \qmltype PathAnimation
    \instantiates QQuickPathAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-properties
    \inherits Animation
    \since 5.0
    \brief Animates an item along a path

    When used in a transition, the path can be specified without start
    or end points, for example:
    \qml
    PathAnimation {
        path: Path {
            //no startX, startY
            PathCurve { x: 100; y: 100}
            PathCurve {}    //last element is empty with no end point specified
        }
    }
    \endqml

    In the above case, the path start will be the item's current position, and the
    path end will be the item's target position in the target state.

    \sa {Animation and Transitions in Qt Quick}, PathInterpolator
*/
QQuickPathAnimation::QQuickPathAnimation(QObject *parent)
: QQuickAbstractAnimation(*(new QQuickPathAnimationPrivate), parent)
{
}

QQuickPathAnimation::~QQuickPathAnimation()
{
    typedef QHash<QQuickItem*, QQuickPathAnimationAnimator* >::iterator ActiveAnimationsIt;

    Q_D(QQuickPathAnimation);
    for (ActiveAnimationsIt it = d->activeAnimations.begin(), end = d->activeAnimations.end(); it != end; ++it)
        it.value()->clearTemplate();
}

/*!
    \qmlproperty int QtQuick::PathAnimation::duration
    This property holds the duration of the animation, in milliseconds.

    The default value is 250.
*/
int QQuickPathAnimation::duration() const
{
    Q_D(const QQuickPathAnimation);
    return d->duration;
}

void QQuickPathAnimation::setDuration(int duration)
{
    if (duration < 0) {
        qmlInfo(this) << tr("Cannot set a duration of < 0");
        return;
    }

    Q_D(QQuickPathAnimation);
    if (d->duration == duration)
        return;
    d->duration = duration;
    emit durationChanged(duration);
}

/*!
    \qmlpropertygroup QtQuick::PathAnimation::easing
    \qmlproperty enumeration QtQuick::PathAnimation::easing.type
    \qmlproperty real QtQuick::PathAnimation::easing.amplitude
    \qmlproperty list<real> QtQuick::PathAnimation::easing.bezierCurve
    \qmlproperty real QtQuick::PathAnimation::easing.overshoot
    \qmlproperty real QtQuick::PathAnimation::easing.period
    \brief the easing curve used for the animation.

    To specify an easing curve you need to specify at least the type. For some curves you can also specify
    amplitude, period, overshoot or custom bezierCurve data. The default easing curve is \c Easing.Linear.

    See the \l{PropertyAnimation::easing.type} documentation for information
    about the different types of easing curves.
*/
QEasingCurve QQuickPathAnimation::easing() const
{
    Q_D(const QQuickPathAnimation);
    return d->easingCurve;
}

void QQuickPathAnimation::setEasing(const QEasingCurve &e)
{
    Q_D(QQuickPathAnimation);
    if (d->easingCurve == e)
        return;

    d->easingCurve = e;
    emit easingChanged(e);
}

/*!
    \qmlproperty Path QtQuick::PathAnimation::path
    This property holds the path to animate along.

    For more information on defining a path see the \l Path documentation.
*/
QQuickPath *QQuickPathAnimation::path() const
{
    Q_D(const QQuickPathAnimation);
    return d->path;
}

void QQuickPathAnimation::setPath(QQuickPath *path)
{
    Q_D(QQuickPathAnimation);
    if (d->path == path)
        return;

    d->path = path;
    emit pathChanged();
}

/*!
    \qmlproperty Item QtQuick::PathAnimation::target
    This property holds the item to animate.
*/
QQuickItem *QQuickPathAnimation::target() const
{
    Q_D(const QQuickPathAnimation);
    return d->target;
}

void QQuickPathAnimation::setTargetObject(QQuickItem *target)
{
    Q_D(QQuickPathAnimation);
    if (d->target == target)
        return;

    d->target = target;
    emit targetChanged();
}

/*!
    \qmlproperty enumeration QtQuick::PathAnimation::orientation
    This property controls the rotation of the item as it animates along the path.

    If a value other than \c Fixed is specified, the PathAnimation will rotate the
    item to achieve the specified orientation as it travels along the path.

    \list
    \li PathAnimation.Fixed (default) - the PathAnimation will not control
       the rotation of the item.
    \li PathAnimation.RightFirst - The right side of the item will lead along the path.
    \li PathAnimation.LeftFirst - The left side of the item will lead along the path.
    \li PathAnimation.BottomFirst - The bottom of the item will lead along the path.
    \li PathAnimation.TopFirst - The top of the item will lead along the path.
    \endlist
*/
QQuickPathAnimation::Orientation QQuickPathAnimation::orientation() const
{
    Q_D(const QQuickPathAnimation);
    return d->orientation;
}

void QQuickPathAnimation::setOrientation(Orientation orientation)
{
    Q_D(QQuickPathAnimation);
    if (d->orientation == orientation)
        return;

    d->orientation = orientation;
    emit orientationChanged(d->orientation);
}

/*!
    \qmlproperty point QtQuick::PathAnimation::anchorPoint
    This property holds the anchor point for the item being animated.

    By default, the upper-left corner of the target (its 0,0 point)
    will be anchored to (or follow) the path. The anchorPoint property can be used to
    specify a different point for anchoring. For example, specifying an anchorPoint of
    5,5 for a 10x10 item means the center of the item will follow the path.
*/
QPointF QQuickPathAnimation::anchorPoint() const
{
    Q_D(const QQuickPathAnimation);
    return d->anchorPoint;
}

void QQuickPathAnimation::setAnchorPoint(const QPointF &point)
{
    Q_D(QQuickPathAnimation);
    if (d->anchorPoint == point)
        return;

    d->anchorPoint = point;
    emit anchorPointChanged(point);
}

/*!
    \qmlproperty real QtQuick::PathAnimation::orientationEntryDuration
    This property holds the duration (in milliseconds) of the transition in to the orientation.

    If an orientation has been specified for the PathAnimation, and the starting
    rotation of the item does not match that given by the orientation,
    orientationEntryDuration can be used to smoothly transition from the item's
    starting rotation to the rotation given by the path orientation.
*/
int QQuickPathAnimation::orientationEntryDuration() const
{
    Q_D(const QQuickPathAnimation);
    return d->entryDuration;
}

void QQuickPathAnimation::setOrientationEntryDuration(int duration)
{
    Q_D(QQuickPathAnimation);
    if (d->entryDuration == duration)
        return;
    d->entryDuration = duration;
    emit orientationEntryDurationChanged(duration);
}

/*!
    \qmlproperty real QtQuick::PathAnimation::orientationExitDuration
    This property holds the duration (in milliseconds) of the transition out of the orientation.

    If an orientation and endRotation have been specified for the PathAnimation,
    orientationExitDuration can be used to smoothly transition from the rotation given
    by the path orientation to the specified endRotation.
*/
int QQuickPathAnimation::orientationExitDuration() const
{
    Q_D(const QQuickPathAnimation);
    return d->exitDuration;
}

void QQuickPathAnimation::setOrientationExitDuration(int duration)
{
    Q_D(QQuickPathAnimation);
    if (d->exitDuration == duration)
        return;
    d->exitDuration = duration;
    emit orientationExitDurationChanged(duration);
}

/*!
    \qmlproperty real QtQuick::PathAnimation::endRotation
    This property holds the ending rotation for the target.

    If an orientation has been specified for the PathAnimation,
    and the path doesn't end with the item at the desired rotation,
    the endRotation property can be used to manually specify an end
    rotation.

    This property is typically used with orientationExitDuration, as specifying
    an endRotation without an orientationExitDuration may cause a jump to
    the final rotation, rather than a smooth transition.
*/
qreal QQuickPathAnimation::endRotation() const
{
    Q_D(const QQuickPathAnimation);
    return d->endRotation.isNull ? qreal(0) : d->endRotation.value;
}

void QQuickPathAnimation::setEndRotation(qreal rotation)
{
    Q_D(QQuickPathAnimation);
    if (!d->endRotation.isNull && d->endRotation == rotation)
        return;

    d->endRotation = rotation;
    emit endRotationChanged(d->endRotation);
}

QAbstractAnimationJob* QQuickPathAnimation::transition(QQuickStateActions &actions,
                                           QQmlProperties &modified,
                                           TransitionDirection direction,
                                           QObject *defaultTarget)
{
    Q_D(QQuickPathAnimation);

    QQuickItem *target = d->target ? d->target : qobject_cast<QQuickItem*>(defaultTarget);

    QQuickPathAnimationUpdater prevData;
    bool havePrevData = false;
    if (d->activeAnimations.contains(target)) {
        havePrevData = true;
        prevData = *d->activeAnimations[target]->pathUpdater();
    }

    for (auto it = d->activeAnimations.begin(); it != d->activeAnimations.end();) {
        QQuickPathAnimationAnimator *anim = it.value();
        if (anim->state() == QAbstractAnimationJob::Stopped) {
            anim->clearTemplate();
            it = d->activeAnimations.erase(it);
        } else {
            ++it;
        }
    }

    QQuickPathAnimationUpdater *data = new QQuickPathAnimationUpdater();
    QQuickPathAnimationAnimator *pa = new QQuickPathAnimationAnimator(d);

    d->activeAnimations[target] = pa;

    data->orientation = d->orientation;
    data->anchorPoint = d->anchorPoint;
    data->entryInterval = d->duration ? qreal(d->entryDuration) / d->duration : qreal(0);
    data->exitInterval = d->duration ? qreal(d->exitDuration) / d->duration : qreal(0);
    data->endRotation = d->endRotation;
    data->reverse = direction == Backward ? true : false;
    data->fromSourced = false;
    data->fromDefined = (d->path && d->path->hasStartX() && d->path->hasStartY()) ? true : false;
    data->toDefined = d->path ? d->path->hasEnd() : false;
    int origModifiedSize = modified.count();

    for (int i = 0; i < actions.count(); ++i) {
        QQuickStateAction &action = actions[i];
        if (action.event)
            continue;
        if (action.specifiedObject == target && action.property.name() == QLatin1String("x")) {
            data->toX = action.toValue.toReal();
            modified << action.property;
            action.fromValue = action.toValue;
        }
        if (action.specifiedObject == target && action.property.name() == QLatin1String("y")) {
            data->toY = action.toValue.toReal();
            modified << action.property;
            action.fromValue = action.toValue;
        }
    }

    if (target && d->path &&
        (modified.count() > origModifiedSize || data->toDefined)) {
        data->target = target;
        data->path = d->path;
        data->path->invalidateSequentialHistory();

        if (havePrevData) {
            // get the original start angle that was used (so we can exactly reverse).
            data->startRotation = prevData.startRotation;

            // treat interruptions specially, otherwise we end up with strange paths
            if ((data->reverse || prevData.reverse) && prevData.currentV > 0 && prevData.currentV < 1) {
                if (!data->fromDefined && !data->toDefined && !prevData.painterPath.isEmpty()) {
                    QPointF pathPos = QQuickPath::sequentialPointAt(prevData.painterPath, prevData.pathLength, prevData.attributePoints, prevData.prevBez, prevData.currentV);
                    if (!prevData.anchorPoint.isNull())
                        pathPos -= prevData.anchorPoint;
                    if (pathPos == data->target->position()) {   //only treat as interruption if we interrupted ourself
                        data->painterPath = prevData.painterPath;
                        data->toDefined = data->fromDefined = data->fromSourced = true;
                        data->prevBez.isValid = false;
                        data->interruptStart = prevData.currentV;
                        data->startRotation = prevData.startRotation;
                        data->pathLength = prevData.pathLength;
                        data->attributePoints = prevData.attributePoints;
                    }
                }
            }
        }
        pa->setFromSourcedValue(&data->fromSourced);
        pa->setAnimValue(data);
        pa->setDuration(d->duration);
        pa->setEasingCurve(d->easingCurve);
        return initInstance(pa);
    } else {
        pa->setFromSourcedValue(0);
        pa->setAnimValue(0);
        delete pa;
        delete data;
    }
    return 0;
}

void QQuickPathAnimationUpdater::setValue(qreal v)
{
    v = qMin(qMax(v, (qreal)0.0), (qreal)1.0);;

    if (interruptStart.isValid()) {
        if (reverse)
            v = 1 - v;
        qreal end = reverse ? 0.0 : 1.0;
        v = interruptStart + v * (end-interruptStart);
    }
    currentV = v;
    bool atStart = ((reverse && v == 1.0) || (!reverse && v == 0.0));
    if (!fromSourced && (!fromDefined || !toDefined)) {
        qreal startX = reverse ? toX + anchorPoint.x() : target->x() + anchorPoint.x();
        qreal startY = reverse ? toY + anchorPoint.y() : target->y() + anchorPoint.y();
        qreal endX = reverse ? target->x() + anchorPoint.x() : toX + anchorPoint.x();
        qreal endY = reverse ? target->y() + anchorPoint.y() : toY + anchorPoint.y();

        prevBez.isValid = false;
        painterPath = path->createPath(QPointF(startX, startY), QPointF(endX, endY), QStringList(), pathLength, attributePoints);
        fromSourced = true;
    }

    qreal angle;
    bool fixed = orientation == QQuickPathAnimation::Fixed;
    QPointF currentPos = !painterPath.isEmpty() ? path->sequentialPointAt(painterPath, pathLength, attributePoints, prevBez, v, fixed ? 0 : &angle) : path->sequentialPointAt(v, fixed ? 0 : &angle);

    //adjust position according to anchor point
    if (!anchorPoint.isNull()) {
        currentPos -= anchorPoint;
        if (atStart) {
            if (!anchorPoint.isNull() && !fixed)
                target->setTransformOriginPoint(anchorPoint);
        }
    }

    target->setPosition(currentPos);

    //adjust angle according to orientation
    if (!fixed) {
        switch (orientation) {
            case QQuickPathAnimation::RightFirst:
                angle = -angle;
                break;
            case QQuickPathAnimation::TopFirst:
                angle = -angle + 90;
                break;
            case QQuickPathAnimation::LeftFirst:
                angle = -angle + 180;
                break;
            case QQuickPathAnimation::BottomFirst:
                angle = -angle + 270;
                break;
            default:
                angle = 0;
                break;
        }

        if (atStart && !reverse) {
            startRotation = target->rotation();

            //shortest distance to correct orientation
            qreal diff = angle - startRotation;
            while (diff > 180.0) {
                startRotation.value += 360.0;
                diff -= 360.0;
            }
            while (diff < -180.0) {
                startRotation.value -= 360.0;
                diff += 360.0;
            }
        }

        //smoothly transition to the desired orientation
        //TODO: shortest distance calculations
        if (startRotation.isValid()) {
            if (reverse && v == 0.0)
                angle = startRotation;
            else if (v < entryInterval)
                angle = angle * v / entryInterval + startRotation * (entryInterval - v) / entryInterval;
        }
        if (endRotation.isValid()) {
            qreal exitStart = 1 - entryInterval;
            if (!reverse && v == 1.0)
                angle = endRotation;
            else if (v > exitStart)
                angle = endRotation * (v - exitStart) / exitInterval + angle * (exitInterval - (v - exitStart)) / exitInterval;
        }
        target->setRotation(angle);
    }

    /*
        NOTE: we don't always reset the transform origin, as it can cause a
        visual jump if ending on an angle. This means that in some cases
        (anchor point and orientation both specified, and ending at an angle)
        the transform origin will always be set after running the path animation.
     */
    if ((reverse && v == 0.0) || (!reverse && v == 1.0)) {
        if (!anchorPoint.isNull() && !fixed && qFuzzyIsNull(angle))
            target->setTransformOriginPoint(QPointF());
    }
}

QQuickPathAnimationAnimator::QQuickPathAnimationAnimator(QQuickPathAnimationPrivate *priv)
    : animationTemplate(priv)
{
}

QQuickPathAnimationAnimator::~QQuickPathAnimationAnimator()
{
    if (animationTemplate && pathUpdater()) {
        QHash<QQuickItem*, QQuickPathAnimationAnimator* >::iterator it =
                animationTemplate->activeAnimations.find(pathUpdater()->target);
        if (it != animationTemplate->activeAnimations.end() && it.value() == this)
            animationTemplate->activeAnimations.erase(it);
    }
}

#endif // quick_path

QT_END_NAMESPACE
