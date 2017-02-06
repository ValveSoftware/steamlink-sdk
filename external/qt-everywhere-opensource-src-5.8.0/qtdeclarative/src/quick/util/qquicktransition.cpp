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

#include "qquicktransition_p.h"

#include "qquickstate_p.h"
#include "qquickstate_p_p.h"
#include "qquickstatechangescript_p.h"
#include "qquickanimation_p.h"
#include "qquickanimation_p_p.h"
#include "qquicktransitionmanager_p_p.h"

#include <private/qquickanimatorjob_p.h>

#include "private/qparallelanimationgroupjob_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype Transition
    \instantiates QQuickTransition
    \inqmlmodule QtQuick
    \ingroup qtquick-transitions-animations
    \brief Defines animated transitions that occur on state changes

    A Transition defines the animations to be applied when a \l State change occurs.

    For example, the following \l Rectangle has two states: the default state, and
    an added "moved" state. In the "moved state, the rectangle's position changes
    to (50, 50).  The added Transition specifies that when the rectangle
    changes between the default and the "moved" state, any changes
    to the \c x and \c y properties should be animated, using an \c Easing.InOutQuad.

    \snippet qml/transition.qml 0

    Notice the example does not require \l{PropertyAnimation::}{to} and
    \l{PropertyAnimation::}{from} values for the NumberAnimation. As a convenience,
    these properties are automatically set to the values of \c x and \c y before
    and after the state change; the \c from values are provided by
    the current values of \c x and \c y, and the \c to values are provided by
    the PropertyChanges object. If you wish, you can provide \l{PropertyAnimation::}{to} and
    \l{PropertyAnimation::}{from} values anyway to override the default values.

    By default, a Transition's animations are applied for any state change in the
    parent item. The  Transition \l {Transition::}{from} and \l {Transition::}{to}
    values can be set to restrict the animations to only be applied when changing
    from one particular state to another.

    To define multiple transitions, specify \l Item::transitions as a list:

    \snippet qml/transitions-list.qml list of transitions

    If multiple Transitions are specified, only a single (best-matching) Transition will be applied for any particular
    state change. In the example above, when changing to \c state1, the first transition will be used, rather
    than the more generic second transition.

    If a state change has a Transition that matches the same property as a
    \l Behavior, the Transition animation overrides the \l Behavior for that
    state change.

    \sa {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation#States}{States example}, {Qt Quick States}, {Qt QML}
*/

//ParallelAnimationWrapper allows us to do a "callback" when the animation finishes, rather than connecting
//and disconnecting signals and slots frequently
class ParallelAnimationWrapper : public QParallelAnimationGroupJob
{
public:
    ParallelAnimationWrapper() : QParallelAnimationGroupJob() {}
    QQuickTransitionManager *manager;

protected:
    virtual void updateState(QAbstractAnimationJob::State newState, QAbstractAnimationJob::State oldState);
};

class QQuickTransitionPrivate : public QObjectPrivate, QAnimationJobChangeListener
{
    Q_DECLARE_PUBLIC(QQuickTransition)
public:
    QQuickTransitionPrivate()
    : fromState(QLatin1String("*")), toState(QLatin1String("*"))
    , runningInstanceCount(0), state(QAbstractAnimationJob::Stopped)
    , reversed(false), reversible(false), enabled(true)
    {
    }

    void removeStateChangeListener(QAbstractAnimationJob *anim)
    {
        if (anim)
            anim->removeAnimationChangeListener(this, QAbstractAnimationJob::StateChange);
    }

    QString fromState;
    QString toState;
    quint32 runningInstanceCount;
    QAbstractAnimationJob::State state;
    bool reversed;
    bool reversible;
    bool enabled;
protected:
    virtual void animationStateChanged(QAbstractAnimationJob *, QAbstractAnimationJob::State, QAbstractAnimationJob::State);

    static void append_animation(QQmlListProperty<QQuickAbstractAnimation> *list, QQuickAbstractAnimation *a);
    static int animation_count(QQmlListProperty<QQuickAbstractAnimation> *list);
    static QQuickAbstractAnimation* animation_at(QQmlListProperty<QQuickAbstractAnimation> *list, int pos);
    static void clear_animations(QQmlListProperty<QQuickAbstractAnimation> *list);
    QList<QQuickAbstractAnimation *> animations;
};

void QQuickTransitionPrivate::append_animation(QQmlListProperty<QQuickAbstractAnimation> *list, QQuickAbstractAnimation *a)
{
    QQuickTransition *q = static_cast<QQuickTransition *>(list->object);
    q->d_func()->animations.append(a);
    a->setDisableUserControl();
}

int QQuickTransitionPrivate::animation_count(QQmlListProperty<QQuickAbstractAnimation> *list)
{
    QQuickTransition *q = static_cast<QQuickTransition *>(list->object);
    return q->d_func()->animations.count();
}

QQuickAbstractAnimation* QQuickTransitionPrivate::animation_at(QQmlListProperty<QQuickAbstractAnimation> *list, int pos)
{
    QQuickTransition *q = static_cast<QQuickTransition *>(list->object);
    return q->d_func()->animations.at(pos);
}

void QQuickTransitionPrivate::clear_animations(QQmlListProperty<QQuickAbstractAnimation> *list)
{
    QQuickTransition *q = static_cast<QQuickTransition *>(list->object);
    while (q->d_func()->animations.count()) {
        QQuickAbstractAnimation *firstAnim = q->d_func()->animations.at(0);
        q->d_func()->animations.removeAll(firstAnim);
    }
}

void QQuickTransitionPrivate::animationStateChanged(QAbstractAnimationJob *, QAbstractAnimationJob::State newState, QAbstractAnimationJob::State)
{
    Q_Q(QQuickTransition);

    if (newState == QAbstractAnimationJob::Running) {
        runningInstanceCount++;
        if (runningInstanceCount == 1)
            emit q->runningChanged();
    } else if (newState == QAbstractAnimationJob::Stopped) {
        runningInstanceCount--;
        if (runningInstanceCount == 0)
            emit q->runningChanged();
    }
}

void ParallelAnimationWrapper::updateState(QAbstractAnimationJob::State newState, QAbstractAnimationJob::State oldState)
{
    QParallelAnimationGroupJob::updateState(newState, oldState);
    if (newState == Stopped && (duration() == -1
        || (direction() == QAbstractAnimationJob::Forward && currentLoopTime() == duration())
        || (direction() == QAbstractAnimationJob::Backward && currentLoopTime() == 0)))
    {
         manager->complete();
    }
}

QQuickTransitionInstance::QQuickTransitionInstance(QQuickTransitionPrivate *transition, QAbstractAnimationJob *anim)
    : m_transition(transition)
    , m_anim(anim)
{
}

QQuickTransitionInstance::~QQuickTransitionInstance()
{
    m_transition->removeStateChangeListener(m_anim);
    delete m_anim;
}

void QQuickTransitionInstance::start()
{
    if (m_anim)
        m_anim->start();
}

void QQuickTransitionInstance::stop()
{
    if (m_anim)
        m_anim->stop();
}

bool QQuickTransitionInstance::isRunning() const
{
    return m_anim && m_anim->state() == QAbstractAnimationJob::Running;
}

QQuickTransition::QQuickTransition(QObject *parent)
    : QObject(*(new QQuickTransitionPrivate), parent)
{
}

QQuickTransition::~QQuickTransition()
{
}

void QQuickTransition::setReversed(bool r)
{
    Q_D(QQuickTransition);
    d->reversed = r;
}

QQuickTransitionInstance *QQuickTransition::prepare(QQuickStateOperation::ActionList &actions,
                            QList<QQmlProperty> &after,
                            QQuickTransitionManager *manager,
                            QObject *defaultTarget)
{
    Q_D(QQuickTransition);

    qmlExecuteDeferred(this);

    ParallelAnimationWrapper *group = new ParallelAnimationWrapper();
    group->manager = manager;

    QQuickAbstractAnimation::TransitionDirection direction = d->reversed ? QQuickAbstractAnimation::Backward : QQuickAbstractAnimation::Forward;
    int start = d->reversed ? d->animations.count() - 1 : 0;
    int end = d->reversed ? -1 : d->animations.count();

    QAbstractAnimationJob *anim = 0;
    for (int i = start; i != end;) {
        anim = d->animations.at(i)->transition(actions, after, direction, defaultTarget);
        if (anim) {
            if (d->animations.at(i)->threadingModel() == QQuickAbstractAnimation::RenderThread)
                anim = new QQuickAnimatorProxyJob(anim, d->animations.at(i));
            d->reversed ? group->prependAnimation(anim) : group->appendAnimation(anim);
        }
        d->reversed ? --i : ++i;
    }

    group->setDirection(d->reversed ? QAbstractAnimationJob::Backward : QAbstractAnimationJob::Forward);

    group->addAnimationChangeListener(d, QAbstractAnimationJob::StateChange);
    QQuickTransitionInstance *wrapper = new QQuickTransitionInstance(d, group);
    return wrapper;
}

/*!
    \qmlproperty string QtQuick::Transition::from
    \qmlproperty string QtQuick::Transition::to

    These properties indicate the state changes that trigger the transition.

    The default values for these properties is "*" (that is, any state).

    For example, the following transition has not set the \c to and \c from
    properties, so the animation is always applied when changing between
    the two states (i.e. when the mouse is pressed and released).

    \snippet qml/transition-from-to.qml 0

    If the transition was changed to this:

    \snippet qml/transition-from-to-modified.qml modified transition

    The animation would only be applied when changing from the default state to
    the "brighter" state (i.e. when the mouse is pressed, but not on release).

    Multiple \c to and \c from values can be set by using a comma-separated string.

    \sa reversible
*/
QString QQuickTransition::fromState() const
{
    Q_D(const QQuickTransition);
    return d->fromState;
}

void QQuickTransition::setFromState(const QString &f)
{
    Q_D(QQuickTransition);
    if (f == d->fromState)
        return;

    d->fromState = f;
    emit fromChanged();
}

/*!
    \qmlproperty bool QtQuick::Transition::reversible
    This property holds whether the transition should be automatically reversed when the conditions that triggered this transition are reversed.

    The default value is false.

    By default, transitions run in parallel and are applied to all state
    changes if the \l from and \l to states have not been set. In this
    situation, the transition is automatically applied when a state change
    is reversed, and it is not necessary to set this property to reverse
    the transition.

    However, if a SequentialAnimation is used, or if the \l from or \l to
    properties have been set, this property will need to be set to reverse
    a transition when a state change is reverted. For example, the following
    transition applies a sequential animation when the mouse is pressed,
    and reverses the sequence of the animation when the mouse is released:

    \snippet qml/transition-reversible.qml 0

    If the transition did not set the \c to and \c reversible values, then
    on the mouse release, the transition would play the PropertyAnimation
    before the ColorAnimation instead of reversing the sequence.
*/
bool QQuickTransition::reversible() const
{
    Q_D(const QQuickTransition);
    return d->reversible;
}

void QQuickTransition::setReversible(bool r)
{
    Q_D(QQuickTransition);
    if (r == d->reversible)
        return;

    d->reversible = r;
    emit reversibleChanged();
}

QString QQuickTransition::toState() const
{
    Q_D(const QQuickTransition);
    return d->toState;
}

void QQuickTransition::setToState(const QString &t)
{
    Q_D(QQuickTransition);
    if (t == d->toState)
        return;

    d->toState = t;
    emit toChanged();
}

/*!
    \qmlproperty bool QtQuick::Transition::enabled

    This property holds whether the Transition will be run when moving
    from the \c from state to the \c to state.

    By default a Transition is enabled.

    Note that in some circumstances disabling a Transition may cause an
    alternative Transition to be used in its place. In the following
    example, although the first Transition has been set to animate changes
    from "state1" to "state2", this transition has been disabled by setting
    \c enabled to \c false, so any such state change will actually be animated
    by the second Transition instead.

    \qml
    Item {
        states: [
            State { name: "state1" },
            State { name: "state2" }
        ]
        transitions: [
            Transition { from: "state1"; to: "state2"; enabled: false },
            Transition {
                // ...
            }
        ]
    }
    \endqml
*/

bool QQuickTransition::enabled() const
{
    Q_D(const QQuickTransition);
    return d->enabled;
}

void QQuickTransition::setEnabled(bool enabled)
{
    Q_D(QQuickTransition);
    if (d->enabled == enabled)
        return;
    d->enabled = enabled;
    emit enabledChanged();
}

/*!
    \qmlproperty bool QtQuick::Transition::running

    This property holds whether the transition is currently running.

    This property is read only.
*/
bool QQuickTransition::running() const
{
    Q_D(const QQuickTransition);
    return d->runningInstanceCount;
}


/*!
    \qmlproperty list<Animation> QtQuick::Transition::animations
    \default

    This property holds a list of the animations to be run for this transition.

    \snippet ../qml/dynamicscene/dynamicscene.qml top-level transitions

    The top-level animations are run in parallel. To run them sequentially,
    define them within a SequentialAnimation:

    \snippet qml/transition-reversible.qml sequential animations
*/
QQmlListProperty<QQuickAbstractAnimation> QQuickTransition::animations()
{
    Q_D(QQuickTransition);
    return QQmlListProperty<QQuickAbstractAnimation>(this, &d->animations, QQuickTransitionPrivate::append_animation,
                                                                   QQuickTransitionPrivate::animation_count,
                                                                   QQuickTransitionPrivate::animation_at,
                                                                   QQuickTransitionPrivate::clear_animations);
}

QT_END_NAMESPACE

//#include <qquicktransition.moc>
