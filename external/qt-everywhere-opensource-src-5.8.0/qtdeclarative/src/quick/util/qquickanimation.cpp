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

#include "qquickanimation_p.h"
#include "qquickanimation_p_p.h"

#include "qquickanimatorjob_p.h"

#include <private/qquickstatechangescript_p.h>
#include <private/qqmlcontext_p.h>

#include <qqmlpropertyvaluesource.h>
#include <qqml.h>
#include <qqmlinfo.h>
#include <qqmlexpression.h>
#include <private/qqmlstringconverters_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qqmlmetatype_p.h>
#include <private/qqmlvaluetype_p.h>
#include <private/qqmlproperty_p.h>
#include <private/qqmlengine_p.h>

#include <qvariant.h>
#include <qcolor.h>
#include <qfile.h>
#include "private/qparallelanimationgroupjob_p.h"
#include "private/qsequentialanimationgroupjob_p.h"
#include <QtCore/qset.h>
#include <QtCore/qrect.h>
#include <QtCore/qpoint.h>
#include <QtCore/qsize.h>
#include <QtCore/qmath.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Animation
    \instantiates QQuickAbstractAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-transitions-animations
    \brief Is the base of all QML animations

    The Animation type cannot be used directly in a QML file.  It exists
    to provide a set of common properties and methods, available across all the
    other animation types that inherit from it.  Attempting to use the Animation
    type directly will result in an error.
*/

QQuickAbstractAnimation::QQuickAbstractAnimation(QObject *parent)
: QObject(*(new QQuickAbstractAnimationPrivate), parent)
{
}

QQuickAbstractAnimation::~QQuickAbstractAnimation()
{
    Q_D(QQuickAbstractAnimation);
    if (d->group)
        setGroup(0);    //remove from group
    delete d->animationInstance;
}

QQuickAbstractAnimation::QQuickAbstractAnimation(QQuickAbstractAnimationPrivate &dd, QObject *parent)
: QObject(dd, parent)
{
}

QAbstractAnimationJob* QQuickAbstractAnimation::qtAnimation()
{
    Q_D(QQuickAbstractAnimation);
    return d->animationInstance;
}

/*!
    \qmlproperty bool QtQuick::Animation::running
    This property holds whether the animation is currently running.

    The \c running property can be set to declaratively control whether or not
    an animation is running.  The following example will animate a rectangle
    whenever the \l MouseArea is pressed.

    \code
    Rectangle {
        width: 100; height: 100
        NumberAnimation on x {
            running: myMouse.pressed
            from: 0; to: 100
        }
        MouseArea { id: myMouse }
    }
    \endcode

    Likewise, the \c running property can be read to determine if the animation
    is running.  In the following example the Text item will indicate whether
    or not the animation is running.

    \code
    NumberAnimation { id: myAnimation }
    Text { text: myAnimation.running ? "Animation is running" : "Animation is not running" }
    \endcode

    Animations can also be started and stopped imperatively from JavaScript
    using the \c start() and \c stop() methods.

    By default, animations are not running. Though, when the animations are assigned to properties,
    as property value sources using the \e on syntax, they are set to running by default.
*/
bool QQuickAbstractAnimation::isRunning() const
{
    Q_D(const QQuickAbstractAnimation);
    return d->running;
}

// the behavior calls this function
void QQuickAbstractAnimation::notifyRunningChanged(bool running)
{
    Q_D(QQuickAbstractAnimation);
    if (d->disableUserControl && d->running != running) {
        d->running = running;
        emit runningChanged(running);
    }
}

//commence is called to start an animation when it is used as a
//simple animation, and not as part of a transition
void QQuickAbstractAnimationPrivate::commence()
{
    Q_Q(QQuickAbstractAnimation);

    QQuickStateActions actions;
    QQmlProperties properties;

    QAbstractAnimationJob *oldInstance = animationInstance;
    animationInstance = q->transition(actions, properties, QQuickAbstractAnimation::Forward);
    if (oldInstance && oldInstance != animationInstance)
        delete oldInstance;

    if (animationInstance) {
        if (oldInstance != animationInstance) {
            if (q->threadingModel() == QQuickAbstractAnimation::RenderThread)
                animationInstance = new QQuickAnimatorProxyJob(animationInstance, q);
            animationInstance->addAnimationChangeListener(this, QAbstractAnimationJob::Completion);
        }
        animationInstance->start();
        if (animationInstance->isStopped()) {
            running = false;
            emit q->stopped();
        }
    }
}

QQmlProperty QQuickAbstractAnimationPrivate::createProperty(QObject *obj, const QString &str, QObject *infoObj, QString *errorMessage)
{
    QQmlProperty prop(obj, str, qmlContext(infoObj));
    if (!prop.isValid()) {
        const QString message = QQuickAbstractAnimation::tr("Cannot animate non-existent property \"%1\"").arg(str);
        if (errorMessage)
            *errorMessage = message;
        else
            qmlInfo(infoObj) << message;
        return QQmlProperty();
    } else if (!prop.isWritable()) {
        const QString message = QQuickAbstractAnimation::tr("Cannot animate read-only property \"%1\"").arg(str);
        if (errorMessage)
            *errorMessage = message;
        else
            qmlInfo(infoObj) << message;
        return QQmlProperty();
    }
    return prop;
}

/*!
    \qmlsignal QtQuick::Animation::started()

    This signal is emitted when the animation begins.

    It is only triggered for top-level, standalone animations. It will not be
    triggered for animations in a Behavior or Transition, or animations
    that are part of an animation group.

    The corresponding handler is \c onStarted.
*/

/*!
    \qmlsignal QtQuick::Animation::stopped()

    This signal is emitted when the animation ends.

    The animation may have been stopped manually, or may have run to completion.

    It is only triggered for top-level, standalone animations. It will not be
    triggered for animations in a Behavior or Transition, or animations
    that are part of an animation group.

    If \l alwaysRunToEnd is true, this signal will not be emitted until the animation
    has completed its current iteration.

    The corresponding handler is \c onStopped.
*/

void QQuickAbstractAnimation::setRunning(bool r)
{
    Q_D(QQuickAbstractAnimation);
    if (!d->componentComplete) {
        d->running = r;
        if (r == false)
            d->avoidPropertyValueSourceStart = true;
        else if (!d->registered) {
            d->registered = true;
            QQmlEnginePrivate *engPriv = QQmlEnginePrivate::get(qmlEngine(this));
            static int finalizedIdx = -1;
            if (finalizedIdx < 0)
                finalizedIdx = metaObject()->indexOfSlot("componentFinalized()");
            engPriv->registerFinalizeCallback(this, finalizedIdx);
        }
        return;
    }

    if (d->running == r)
        return;

    if (d->group || d->disableUserControl) {
        qmlInfo(this) << "setRunning() cannot be used on non-root animation nodes.";
        return;
    }

    d->running = r;
    if (d->running) {
        bool supressStart = false;
        if (d->alwaysRunToEnd && d->loopCount != 1
            && d->animationInstance && d->animationInstance->isRunning()) {
            //we've restarted before the final loop finished; restore proper loop count
            if (d->loopCount == -1)
                d->animationInstance->setLoopCount(d->loopCount);
            else
                d->animationInstance->setLoopCount(d->animationInstance->currentLoop() + d->loopCount);
            supressStart = true;    //we want the animation to continue, rather than restart
        }
        if (!supressStart) {
            d->commence();
            emit started();
        }
    } else {
        if (d->paused) {
            d->paused = false; //reset paused state to false when stopped
            emit pausedChanged(d->paused);
        }

        if (d->animationInstance) {
            if (d->alwaysRunToEnd) {
                if (d->loopCount != 1)
                    d->animationInstance->setLoopCount(d->animationInstance->currentLoop()+1);    //finish the current loop
            } else {
                d->animationInstance->stop();
                emit stopped();
            }
        }
    }

    emit runningChanged(d->running);
}

/*!
    \qmlproperty bool QtQuick::Animation::paused
    This property holds whether the animation is currently paused.

    The \c paused property can be set to declaratively control whether or not
    an animation is paused.

    Animations can also be paused and resumed imperatively from JavaScript
    using the \c pause() and \c resume() methods.

    By default, animations are not paused.
*/
bool QQuickAbstractAnimation::isPaused() const
{
    Q_D(const QQuickAbstractAnimation);
    Q_ASSERT((d->paused && d->running) || !d->paused);
    return d->paused;
}

void QQuickAbstractAnimation::setPaused(bool p)
{
    Q_D(QQuickAbstractAnimation);
    if (d->paused == p)
        return;

    if (!d->running) {
        qmlInfo(this) << "setPaused() cannot be used when animation isn't running.";
        return;
    }

    if (d->group || d->disableUserControl) {
        qmlInfo(this) << "setPaused() cannot be used on non-root animation nodes.";
        return;
    }

    d->paused = p;

    if (!d->componentComplete || !d->animationInstance)
        return;

    if (d->paused)
        d->animationInstance->pause();
    else
        d->animationInstance->resume();

    emit pausedChanged(d->paused);
}

void QQuickAbstractAnimation::classBegin()
{
    Q_D(QQuickAbstractAnimation);
    d->componentComplete = false;
}

void QQuickAbstractAnimation::componentComplete()
{
    Q_D(QQuickAbstractAnimation);
    d->componentComplete = true;
}

void QQuickAbstractAnimation::componentFinalized()
{
    Q_D(QQuickAbstractAnimation);
    if (d->running) {
        d->running = false;
        setRunning(true);
    }
    if (d->paused) {
        d->paused = false;
        setPaused(true);
    }
}

/*!
    \qmlproperty bool QtQuick::Animation::alwaysRunToEnd
    This property holds whether the animation should run to completion when it is stopped.

    If this true the animation will complete its current iteration when it
    is stopped - either by setting the \c running property to false, or by
    calling the \c stop() method.  The \c complete() method is not effected
    by this value.

    This behavior is most useful when the \c loops property is set, as the
    animation will finish playing normally but not restart.

    By default, the alwaysRunToEnd property is not set.

    \note alwaysRunToEnd has no effect on animations in a Transition.
*/
bool QQuickAbstractAnimation::alwaysRunToEnd() const
{
    Q_D(const QQuickAbstractAnimation);
    return d->alwaysRunToEnd;
}

void QQuickAbstractAnimation::setAlwaysRunToEnd(bool f)
{
    Q_D(QQuickAbstractAnimation);
    if (d->alwaysRunToEnd == f)
        return;

    d->alwaysRunToEnd = f;
    emit alwaysRunToEndChanged(f);
}

/*!
    \qmlproperty int QtQuick::Animation::loops
    This property holds the number of times the animation should play.

    By default, \c loops is 1: the animation will play through once and then stop.

    If set to Animation.Infinite, the animation will continuously repeat until it is explicitly
    stopped - either by setting the \c running property to false, or by calling
    the \c stop() method.

    In the following example, the rectangle will spin indefinitely.

    \code
    Rectangle {
        width: 100; height: 100; color: "green"
        RotationAnimation on rotation {
            loops: Animation.Infinite
            from: 0
            to: 360
        }
    }
    \endcode
*/
int QQuickAbstractAnimation::loops() const
{
    Q_D(const QQuickAbstractAnimation);
    return d->loopCount;
}

void QQuickAbstractAnimation::setLoops(int loops)
{
    Q_D(QQuickAbstractAnimation);
    if (loops < 0)
        loops = -1;

    if (loops == d->loopCount)
        return;

    d->loopCount = loops;
    emit loopCountChanged(loops);
}

int QQuickAbstractAnimation::duration() const
{
    Q_D(const QQuickAbstractAnimation);
    return d->animationInstance ? d->animationInstance->duration() : 0;
}

int QQuickAbstractAnimation::currentTime()
{
    Q_D(QQuickAbstractAnimation);
    return d->animationInstance ? d->animationInstance->currentLoopTime() : 0;
}

void QQuickAbstractAnimation::setCurrentTime(int time)
{
    Q_D(QQuickAbstractAnimation);
    if (d->animationInstance)
        d->animationInstance->setCurrentTime(time);
    //TODO save value for start?
}

QQuickAnimationGroup *QQuickAbstractAnimation::group() const
{
    Q_D(const QQuickAbstractAnimation);
    return d->group;
}

void QQuickAbstractAnimation::setGroup(QQuickAnimationGroup *g)
{
    Q_D(QQuickAbstractAnimation);
    if (d->group == g)
        return;
    if (d->group)
        d->group->d_func()->animations.removeAll(this);

    d->group = g;

    if (d->group && !d->group->d_func()->animations.contains(this))
        d->group->d_func()->animations.append(this);
}

/*!
    \qmlmethod QtQuick::Animation::start()
    \brief Starts the animation

    If the animation is already running, calling this method has no effect.  The
    \c running property will be true following a call to \c start().
*/
void QQuickAbstractAnimation::start()
{
    setRunning(true);
}

/*!
    \qmlmethod QtQuick::Animation::pause()
    \brief Pauses the animation

    If the animation is already paused or not \c running, calling this method has no effect.
    The \c paused property will be true following a call to \c pause().
*/
void QQuickAbstractAnimation::pause()
{
    setPaused(true);
}

/*!
    \qmlmethod QtQuick::Animation::resume()
    \brief Resumes a paused animation

    If the animation is not paused or not \c running, calling this method has no effect.
    The \c paused property will be false following a call to \c resume().
*/
void QQuickAbstractAnimation::resume()
{
    setPaused(false);
}

/*!
    \qmlmethod QtQuick::Animation::stop()
    \brief Stops the animation

    If the animation is not running, calling this method has no effect.  Both the
    \c running and \c paused properties will be false following a call to \c stop().

    Normally \c stop() stops the animation immediately, and the animation has
    no further influence on property values.  In this example animation
    \code
    Rectangle {
        NumberAnimation on x { from: 0; to: 100; duration: 500 }
    }
    \endcode
    was stopped at time 250ms, the \c x property will have a value of 50.

    However, if the \c alwaysRunToEnd property is set, the animation will
    continue running until it completes and then stop.  The \c running property
    will still become false immediately.
*/
void QQuickAbstractAnimation::stop()
{
    setRunning(false);
}

/*!
    \qmlmethod QtQuick::Animation::restart()
    \brief Restarts the animation

    This is a convenience method, and is equivalent to calling \c stop() and
    then \c start().
*/
void QQuickAbstractAnimation::restart()
{
    stop();
    start();
}

/*!
    \qmlmethod QtQuick::Animation::complete()
    \brief Stops the animation, jumping to the final property values

    If the animation is not running, calling this method has no effect.  The
    \c running property will be false following a call to \c complete().

    Unlike \c stop(), \c complete() immediately fast-forwards the animation to
    its end.  In the following example,
    \code
    Rectangle {
        NumberAnimation on x { from: 0; to: 100; duration: 500 }
    }
    \endcode
    calling \c stop() at time 250ms will result in the \c x property having
    a value of 50, while calling \c complete() will set the \c x property to
    100, exactly as though the animation had played the whole way through.
*/
void QQuickAbstractAnimation::complete()
{
    Q_D(QQuickAbstractAnimation);
    if (isRunning() && d->animationInstance) {
         d->animationInstance->setCurrentTime(d->animationInstance->duration());
    }
}

void QQuickAbstractAnimation::setTarget(const QQmlProperty &p)
{
    Q_D(QQuickAbstractAnimation);
    d->defaultProperty = p;

    if (!d->avoidPropertyValueSourceStart)
        setRunning(true);
}

/*
    we rely on setTarget only being called when used as a value source
    so this function allows us to do the same thing as setTarget without
    that assumption
*/
void QQuickAbstractAnimation::setDefaultTarget(const QQmlProperty &p)
{
    Q_D(QQuickAbstractAnimation);
    d->defaultProperty = p;
}

/*
    don't allow start/stop/pause/resume to be manually invoked,
    because something else (like a Behavior) already has control
    over the animation.
*/
void QQuickAbstractAnimation::setDisableUserControl()
{
    Q_D(QQuickAbstractAnimation);
    d->disableUserControl = true;
}

void QQuickAbstractAnimation::setEnableUserControl()
{
    Q_D(QQuickAbstractAnimation);
    d->disableUserControl = false;

}

bool QQuickAbstractAnimation::userControlDisabled() const
{
    Q_D(const QQuickAbstractAnimation);
    return d->disableUserControl;
}

QAbstractAnimationJob* QQuickAbstractAnimation::initInstance(QAbstractAnimationJob *animation)
{
    Q_D(QQuickAbstractAnimation);
    animation->setLoopCount(d->loopCount);
    return animation;
}

QAbstractAnimationJob* QQuickAbstractAnimation::transition(QQuickStateActions &actions,
                                      QQmlProperties &modified,
                                      TransitionDirection direction,
                                      QObject *defaultTarget)
{
    Q_UNUSED(actions);
    Q_UNUSED(modified);
    Q_UNUSED(direction);
    Q_UNUSED(defaultTarget);
    return 0;
}

void QQuickAbstractAnimationPrivate::animationFinished(QAbstractAnimationJob*)
{
    Q_Q(QQuickAbstractAnimation);
    q->setRunning(false);
    if (alwaysRunToEnd) {
        emit q->stopped();
        //restore the proper loopCount for the next run
        if (loopCount != 1)
            animationInstance->setLoopCount(loopCount);
    }
}

QQuickAbstractAnimation::ThreadingModel QQuickAbstractAnimation::threadingModel() const
{
    return GuiThread;
}

/*!
    \qmltype PauseAnimation
    \instantiates QQuickPauseAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-transitions-animations
    \inherits Animation
    \brief Provides a pause for an animation

    When used in a SequentialAnimation, PauseAnimation is a step when
    nothing happens, for a specified duration.

    A 500ms animation sequence, with a 100ms pause between two animations:
    \code
    SequentialAnimation {
        NumberAnimation { ... duration: 200 }
        PauseAnimation { duration: 100 }
        NumberAnimation { ... duration: 200 }
    }
    \endcode

    \sa {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation}
*/
QQuickPauseAnimation::QQuickPauseAnimation(QObject *parent)
: QQuickAbstractAnimation(*(new QQuickPauseAnimationPrivate), parent)
{
}

QQuickPauseAnimation::~QQuickPauseAnimation()
{
}

/*!
    \qmlproperty int QtQuick::PauseAnimation::duration
    This property holds the duration of the pause in milliseconds

    The default value is 250.
*/
int QQuickPauseAnimation::duration() const
{
    Q_D(const QQuickPauseAnimation);
    return d->duration;
}

void QQuickPauseAnimation::setDuration(int duration)
{
    if (duration < 0) {
        qmlInfo(this) << tr("Cannot set a duration of < 0");
        return;
    }

    Q_D(QQuickPauseAnimation);
    if (d->duration == duration)
        return;
    d->duration = duration;
    emit durationChanged(duration);
}

QAbstractAnimationJob* QQuickPauseAnimation::transition(QQuickStateActions &actions,
                                    QQmlProperties &modified,
                                    TransitionDirection direction,
                                    QObject *defaultTarget)
{
    Q_D(QQuickPauseAnimation);
    Q_UNUSED(actions);
    Q_UNUSED(modified);
    Q_UNUSED(direction);
    Q_UNUSED(defaultTarget);

    return initInstance(new QPauseAnimationJob(d->duration));
}

/*!
    \qmltype ColorAnimation
    \instantiates QQuickColorAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-properties
    \inherits PropertyAnimation
    \brief Animates changes in color values

    ColorAnimation is a specialized PropertyAnimation that defines an
    animation to be applied when a color value changes.

    Here is a ColorAnimation applied to the \c color property of a \l Rectangle
    as a property value source. It animates the \c color property's value from
    its current value to a value of "red", over 1000 milliseconds:

    \snippet qml/coloranimation.qml 0

    Like any other animation type, a ColorAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {Animation and Transitions in Qt Quick} documentation shows a
    variety of methods for creating animations.

    For convenience, when a ColorAnimation is used in a \l Transition, it will
    animate any \c color properties that have been modified during the state
    change. If a \l{PropertyAnimation::}{property} or
    \l{PropertyAnimation::}{properties} are explicitly set for the animation,
    then those are used instead.

    \sa {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation}
*/
QQuickColorAnimation::QQuickColorAnimation(QObject *parent)
: QQuickPropertyAnimation(parent)
{
    Q_D(QQuickPropertyAnimation);
    d->interpolatorType = QMetaType::QColor;
    d->defaultToInterpolatorType = true;
    d->interpolator = QVariantAnimationPrivate::getInterpolator(d->interpolatorType);
}

QQuickColorAnimation::~QQuickColorAnimation()
{
}

/*!
    \qmlproperty color QtQuick::ColorAnimation::from
    This property holds the color value at which the animation should begin.

    For example, the following animation is not applied until a color value
    has reached "#c0c0c0":

    \qml
    Item {
        states: [
            // States are defined here...
        ]

        transition: Transition {
            ColorAnimation { from: "#c0c0c0"; duration: 2000 }
        }
    }
    \endqml

    If the ColorAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the starting state of the
    \l Transition, or the current value of the property at the moment the
    \l Behavior is triggered.

    \sa {Animation and Transitions in Qt Quick}
*/
QColor QQuickColorAnimation::from() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->from.value<QColor>();
}

void QQuickColorAnimation::setFrom(const QColor &f)
{
    QQuickPropertyAnimation::setFrom(f);
}

/*!
    \qmlproperty color QtQuick::ColorAnimation::to

    This property holds the color value at which the animation should end.

    If the ColorAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the end state of the
    \l Transition, or the value of the property change that triggered the
    \l Behavior.

    \sa {Animation and Transitions in Qt Quick}
*/
QColor QQuickColorAnimation::to() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->to.value<QColor>();
}

void QQuickColorAnimation::setTo(const QColor &t)
{
    QQuickPropertyAnimation::setTo(t);
}

QActionAnimation::QActionAnimation()
    : QAbstractAnimationJob(), animAction(0)
{
}

QActionAnimation::QActionAnimation(QAbstractAnimationAction *action)
    : QAbstractAnimationJob(), animAction(action)
{
}

QActionAnimation::~QActionAnimation()
{
    delete animAction;
}

int QActionAnimation::duration() const
{
    return 0;
}

void QActionAnimation::setAnimAction(QAbstractAnimationAction *action)
{
    if (isRunning())
        stop();
    animAction = action;
}

void QActionAnimation::updateCurrentTime(int)
{
}

void QActionAnimation::updateState(State newState, State oldState)
{
    Q_UNUSED(oldState);

    if (newState == Running) {
        if (animAction) {
            animAction->doAction();
        }
    }
}

void QActionAnimation::debugAnimation(QDebug d) const
{
    d << "ActionAnimation(" << hex << (const void *) this << dec << ")";

    if (animAction) {
        int indentLevel = 1;
        const QAbstractAnimationJob *job = this;
        while ((job = job->group()))
            ++indentLevel;
        animAction->debugAction(d, indentLevel);
    }
}

/*!
    \qmltype ScriptAction
    \instantiates QQuickScriptAction
    \inqmlmodule QtQuick
    \ingroup qtquick-transitions-animations
    \inherits Animation
    \brief Defines scripts to be run during an animation

    ScriptAction can be used to run a script at a specific point in an animation.

    \qml
    SequentialAnimation {
        NumberAnimation {
            // ...
        }
        ScriptAction { script: doSomething(); }
        NumberAnimation {
            // ...
        }
    }
    \endqml

    When used as part of a Transition, you can also target a specific
    StateChangeScript to run using the \c scriptName property.

    \snippet qml/states/statechangescript.qml state and transition

    \sa StateChangeScript
*/
QQuickScriptAction::QQuickScriptAction(QObject *parent)
    :QQuickAbstractAnimation(*(new QQuickScriptActionPrivate), parent)
{
}

QQuickScriptAction::~QQuickScriptAction()
{
}

QQuickScriptActionPrivate::QQuickScriptActionPrivate()
    : QQuickAbstractAnimationPrivate(), hasRunScriptScript(false), reversing(false){}

/*!
    \qmlproperty script QtQuick::ScriptAction::script
    This property holds the script to run.
*/
QQmlScriptString QQuickScriptAction::script() const
{
    Q_D(const QQuickScriptAction);
    return d->script;
}

void QQuickScriptAction::setScript(const QQmlScriptString &script)
{
    Q_D(QQuickScriptAction);
    d->script = script;
}

/*!
    \qmlproperty string QtQuick::ScriptAction::scriptName
    This property holds the name of the StateChangeScript to run.

    This property is only valid when ScriptAction is used as part of a transition.
    If both script and scriptName are set, scriptName will be used.

    \note When using scriptName in a reversible transition, the script will only
    be run when the transition is being run forwards.
*/
QString QQuickScriptAction::stateChangeScriptName() const
{
    Q_D(const QQuickScriptAction);
    return d->name;
}

void QQuickScriptAction::setStateChangeScriptName(const QString &name)
{
    Q_D(QQuickScriptAction);
    d->name = name;
}

QAbstractAnimationAction* QQuickScriptActionPrivate::createAction()
{
    return new Proxy(this);
}

void QQuickScriptActionPrivate::debugAction(QDebug d, int indentLevel) const
{
    QQmlScriptString scriptStr = hasRunScriptScript ? runScriptScript : script;

    if (!scriptStr.isEmpty()) {
        QQmlExpression expr(scriptStr);

        QByteArray ind(indentLevel, ' ');
        QString exprStr = expr.expression();
        int endOfFirstLine = exprStr.indexOf('\n');
        d << "\n" << ind.constData() << exprStr.leftRef(endOfFirstLine);
        if (endOfFirstLine != -1 && endOfFirstLine < exprStr.length())
            d << "...";
    }
}

void QQuickScriptActionPrivate::execute()
{
    Q_Q(QQuickScriptAction);
    if (hasRunScriptScript && reversing)
        return;

    QQmlScriptString scriptStr = hasRunScriptScript ? runScriptScript : script;

    if (!scriptStr.isEmpty()) {
        QQmlExpression expr(scriptStr);
        expr.evaluate();
        if (expr.hasError())
            qmlInfo(q) << expr.error();
    }
}

QAbstractAnimationJob* QQuickScriptAction::transition(QQuickStateActions &actions,
                                    QQmlProperties &modified,
                                    TransitionDirection direction,
                                    QObject *defaultTarget)
{
    Q_D(QQuickScriptAction);
    Q_UNUSED(modified);
    Q_UNUSED(defaultTarget);

    d->hasRunScriptScript = false;
    d->reversing = (direction == Backward);
    if (!d->name.isEmpty()) {
        for (int ii = 0; ii < actions.count(); ++ii) {
            QQuickStateAction &action = actions[ii];

            if (action.event && action.event->type() == QQuickStateActionEvent::Script
                && static_cast<QQuickStateChangeScript*>(action.event)->name() == d->name) {
                d->runScriptScript = static_cast<QQuickStateChangeScript*>(action.event)->script();
                d->hasRunScriptScript = true;
                action.actionDone = true;
                break;  //only match one (names should be unique)
            }
        }
    }
    return initInstance(new QActionAnimation(d->createAction()));
}

/*!
    \qmltype PropertyAction
    \instantiates QQuickPropertyAction
    \inqmlmodule QtQuick
    \ingroup qtquick-transitions-animations
    \inherits Animation
    \brief Specifies immediate property changes during animation

    PropertyAction is used to specify an immediate property change during an
    animation. The property change is not animated.

    It is useful for setting non-animated property values during an animation.

    For example, here is a SequentialAnimation that sets the image's
    \l {Item::}{opacity} property to \c .5, animates the width of the image,
    then sets \l {Item::}{opacity} back to \c 1:

    \snippet qml/propertyaction.qml standalone

    PropertyAction is also useful for setting the exact point at which a property
    change should occur during a \l Transition. For example, if PropertyChanges
    was used in a \l State to rotate an item around a particular
    \l {Item::}{transformOrigin}, it might be implemented like this:

    \snippet qml/propertyaction.qml transition

    However, with this code, the \c transformOrigin is not set until \e after
    the animation, as a \l State is taken to define the values at the \e end of
    a transition. The animation would rotate at the default \c transformOrigin,
    then jump to \c Item.BottomRight. To fix this, insert a PropertyAction
    before the RotationAnimation begins:

    \snippet qml/propertyaction-sequential.qml sequential

    This immediately sets the \c transformOrigin property to the value defined
    in the end state of the \l Transition (i.e. the value defined in the
    PropertyAction object) so that the rotation animation begins with the
    correct transform origin.

    \sa {Animation and Transitions in Qt Quick}, {Qt QML}
*/
QQuickPropertyAction::QQuickPropertyAction(QObject *parent)
: QQuickAbstractAnimation(*(new QQuickPropertyActionPrivate), parent)
{
}

QQuickPropertyAction::~QQuickPropertyAction()
{
}

QObject *QQuickPropertyAction::target() const
{
    Q_D(const QQuickPropertyAction);
    return d->target;
}

void QQuickPropertyAction::setTargetObject(QObject *o)
{
    Q_D(QQuickPropertyAction);
    if (d->target == o)
        return;
    d->target = o;
    emit targetChanged();
}

QString QQuickPropertyAction::property() const
{
    Q_D(const QQuickPropertyAction);
    return d->propertyName;
}

void QQuickPropertyAction::setProperty(const QString &n)
{
    Q_D(QQuickPropertyAction);
    if (d->propertyName == n)
        return;
    d->propertyName = n;
    emit propertyChanged();
}

/*!
    \qmlproperty Object QtQuick::PropertyAction::target
    \qmlproperty list<Object> QtQuick::PropertyAction::targets
    \qmlproperty string QtQuick::PropertyAction::property
    \qmlproperty string QtQuick::PropertyAction::properties

    These properties determine the items and their properties that are
    affected by this action.

    The details of how these properties are interpreted in different situations
    is covered in the \l{PropertyAnimation::properties}{corresponding} PropertyAnimation
    documentation.

    \sa exclude
*/
QString QQuickPropertyAction::properties() const
{
    Q_D(const QQuickPropertyAction);
    return d->properties;
}

void QQuickPropertyAction::setProperties(const QString &p)
{
    Q_D(QQuickPropertyAction);
    if (d->properties == p)
        return;
    d->properties = p;
    emit propertiesChanged(p);
}

QQmlListProperty<QObject> QQuickPropertyAction::targets()
{
    Q_D(QQuickPropertyAction);
    return QQmlListProperty<QObject>(this, d->targets);
}

/*!
    \qmlproperty list<Object> QtQuick::PropertyAction::exclude
    This property holds the objects that should not be affected by this action.

    \sa targets
*/
QQmlListProperty<QObject> QQuickPropertyAction::exclude()
{
    Q_D(QQuickPropertyAction);
    return QQmlListProperty<QObject>(this, d->exclude);
}

/*!
    \qmlproperty any QtQuick::PropertyAction::value
    This property holds the value to be set on the property.

    If the PropertyAction is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the end state of the
    \l Transition, or the value of the property change that triggered the
    \l Behavior.
*/
QVariant QQuickPropertyAction::value() const
{
    Q_D(const QQuickPropertyAction);
    return d->value;
}

void QQuickPropertyAction::setValue(const QVariant &v)
{
    Q_D(QQuickPropertyAction);
    if (d->value.isNull || d->value != v) {
        d->value = v;
        emit valueChanged(v);
    }
}

QAbstractAnimationJob* QQuickPropertyAction::transition(QQuickStateActions &actions,
                                      QQmlProperties &modified,
                                      TransitionDirection direction,
                                      QObject *defaultTarget)
{
    Q_D(QQuickPropertyAction);
    Q_UNUSED(direction);

    struct QQuickSetPropertyAnimationAction : public QAbstractAnimationAction
    {
        QQuickStateActions actions;
        virtual void doAction()
        {
            for (int ii = 0; ii < actions.count(); ++ii) {
                const QQuickStateAction &action = actions.at(ii);
                QQmlPropertyPrivate::write(action.property, action.toValue, QQmlPropertyData::BypassInterceptor | QQmlPropertyData::DontRemoveBinding);
            }
        }
        virtual void debugAction(QDebug d, int indentLevel) const {
            QByteArray ind(indentLevel, ' ');
            for (int ii = 0; ii < actions.count(); ++ii) {
                const QQuickStateAction &action = actions.at(ii);
                d << "\n" << ind.constData() << "target:" << action.property.object() << "property:" << action.property.name()
                  << "value:" << action.toValue;
            }
        }
    };

    QStringList props = d->properties.isEmpty() ? QStringList() : d->properties.split(QLatin1Char(','));
    for (int ii = 0; ii < props.count(); ++ii)
        props[ii] = props.at(ii).trimmed();
    if (!d->propertyName.isEmpty())
        props << d->propertyName;

    QList<QObject*> targets = d->targets;
    if (d->target)
        targets.append(d->target);

    bool hasSelectors = !props.isEmpty() || !targets.isEmpty() || !d->exclude.isEmpty();

    if (d->defaultProperty.isValid() && !hasSelectors) {
        props << d->defaultProperty.name();
        targets << d->defaultProperty.object();
    }

    if (defaultTarget && targets.isEmpty())
        targets << defaultTarget;

    QQuickSetPropertyAnimationAction *data = new QQuickSetPropertyAnimationAction;

    bool hasExplicit = false;
    //an explicit animation has been specified
    if (d->value.isValid()) {
        for (int i = 0; i < props.count(); ++i) {
            for (int j = 0; j < targets.count(); ++j) {
                QQuickStateAction myAction;
                myAction.property = d->createProperty(targets.at(j), props.at(i), this);
                if (myAction.property.isValid()) {
                    myAction.toValue = d->value;
                    QQuickPropertyAnimationPrivate::convertVariant(myAction.toValue, myAction.property.propertyType());
                    data->actions << myAction;
                    hasExplicit = true;
                    for (int ii = 0; ii < actions.count(); ++ii) {
                        QQuickStateAction &action = actions[ii];
                        if (action.property.object() == myAction.property.object() &&
                            myAction.property.name() == action.property.name()) {
                            modified << action.property;
                            break;  //### any chance there could be multiples?
                        }
                    }
                }
            }
        }
    }

    if (!hasExplicit)
    for (int ii = 0; ii < actions.count(); ++ii) {
        QQuickStateAction &action = actions[ii];

        QObject *obj = action.property.object();
        QString propertyName = action.property.name();
        QObject *sObj = action.specifiedObject;
        QString sPropertyName = action.specifiedProperty;
        bool same = (obj == sObj);

        if ((targets.isEmpty() || targets.contains(obj) || (!same && targets.contains(sObj))) &&
           (!d->exclude.contains(obj)) && (same || (!d->exclude.contains(sObj))) &&
           (props.contains(propertyName) || (!same && props.contains(sPropertyName)))) {
            QQuickStateAction myAction = action;

            if (d->value.isValid())
                myAction.toValue = d->value;
            QQuickPropertyAnimationPrivate::convertVariant(myAction.toValue, myAction.property.propertyType());

            modified << action.property;
            data->actions << myAction;
            action.fromValue = myAction.toValue;
        }
    }

    QActionAnimation *action = new QActionAnimation;
    if (data->actions.count()) {
        action->setAnimAction(data);
    } else {
        delete data;
    }
    return initInstance(action);
}

/*!
    \qmltype NumberAnimation
    \instantiates QQuickNumberAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-properties
    \inherits PropertyAnimation
    \brief Animates changes in qreal-type values

    NumberAnimation is a specialized PropertyAnimation that defines an
    animation to be applied when a numerical value changes.

    Here is a NumberAnimation applied to the \c x property of a \l Rectangle
    as a property value source. It animates the \c x value from its current
    value to a value of 50, over 1000 milliseconds:

    \snippet qml/numberanimation.qml 0

    Like any other animation type, a NumberAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {Animation and Transitions in Qt Quick} documentation shows a
    variety of methods for creating animations.

    Note that NumberAnimation may not animate smoothly if there are irregular
    changes in the number value that it is tracking. If this is the case, use
    SmoothedAnimation instead.

    \sa {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation}
*/
QQuickNumberAnimation::QQuickNumberAnimation(QObject *parent)
: QQuickPropertyAnimation(parent)
{
    init();
}

QQuickNumberAnimation::QQuickNumberAnimation(QQuickPropertyAnimationPrivate &dd, QObject *parent)
: QQuickPropertyAnimation(dd, parent)
{
    init();
}

QQuickNumberAnimation::~QQuickNumberAnimation()
{
}

void QQuickNumberAnimation::init()
{
    Q_D(QQuickPropertyAnimation);
    d->interpolatorType = QMetaType::QReal;
    d->interpolator = QVariantAnimationPrivate::getInterpolator(d->interpolatorType);
}

/*!
    \qmlproperty real QtQuick::NumberAnimation::from
    This property holds the starting value for the animation.

    For example, the following animation is not applied until the \c x value
    has reached 100:

    \qml
    Item {
        states: [
            // ...
        ]

        transition: Transition {
            NumberAnimation { properties: "x"; from: 100; duration: 200 }
        }
    }
    \endqml

    If the NumberAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the starting state of the
    \l Transition, or the current value of the property at the moment the
    \l Behavior is triggered.

    \sa {Animation and Transitions in Qt Quick}
*/

qreal QQuickNumberAnimation::from() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->from.toReal();
}

void QQuickNumberAnimation::setFrom(qreal f)
{
    QQuickPropertyAnimation::setFrom(f);
}

/*!
    \qmlproperty real QtQuick::NumberAnimation::to
    This property holds the end value for the animation.

    If the NumberAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the end state of the
    \l Transition, or the value of the property change that triggered the
    \l Behavior.

    \sa {Animation and Transitions in Qt Quick}
*/
qreal QQuickNumberAnimation::to() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->to.toReal();
}

void QQuickNumberAnimation::setTo(qreal t)
{
    QQuickPropertyAnimation::setTo(t);
}



/*!
    \qmltype Vector3dAnimation
    \instantiates QQuickVector3dAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-properties
    \inherits PropertyAnimation
    \brief Animates changes in QVector3d values

    Vector3dAnimation is a specialized PropertyAnimation that defines an
    animation to be applied when a Vector3d value changes.

    Like any other animation type, a Vector3dAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {Animation and Transitions in Qt Quick} documentation shows a
    variety of methods for creating animations.

    \sa {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation}
*/
QQuickVector3dAnimation::QQuickVector3dAnimation(QObject *parent)
: QQuickPropertyAnimation(parent)
{
    Q_D(QQuickPropertyAnimation);
    d->interpolatorType = QMetaType::QVector3D;
    d->defaultToInterpolatorType = true;
    d->interpolator = QVariantAnimationPrivate::getInterpolator(d->interpolatorType);
}

QQuickVector3dAnimation::~QQuickVector3dAnimation()
{
}

/*!
    \qmlproperty vector3d QtQuick::Vector3dAnimation::from
    This property holds the starting value for the animation.

    If the Vector3dAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the starting state of the
    \l Transition, or the current value of the property at the moment the
    \l Behavior is triggered.

    \sa {Animation and Transitions in Qt Quick}
*/
QVector3D QQuickVector3dAnimation::from() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->from.value<QVector3D>();
}

void QQuickVector3dAnimation::setFrom(QVector3D f)
{
    QQuickPropertyAnimation::setFrom(f);
}

/*!
    \qmlproperty vector3d QtQuick::Vector3dAnimation::to
    This property holds the end value for the animation.

    If the Vector3dAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the end state of the
    \l Transition, or the value of the property change that triggered the
    \l Behavior.

    \sa {Animation and Transitions in Qt Quick}
*/
QVector3D QQuickVector3dAnimation::to() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->to.value<QVector3D>();
}

void QQuickVector3dAnimation::setTo(QVector3D t)
{
    QQuickPropertyAnimation::setTo(t);
}



/*!
    \qmltype RotationAnimation
    \instantiates QQuickRotationAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-properties
    \inherits PropertyAnimation
    \brief Animates changes in rotation values

    RotationAnimation is a specialized PropertyAnimation that gives control
    over the direction of rotation during an animation.

    By default, it rotates in the direction
    of the numerical change; a rotation from 0 to 240 will rotate 240 degrees
    clockwise, while a rotation from 240 to 0 will rotate 240 degrees
    counterclockwise. The \l direction property can be set to specify the
    direction in which the rotation should occur.

    In the following example we use RotationAnimation to animate the rotation
    between states via the shortest path:

    \snippet qml/rotationanimation.qml 0

    Notice the RotationAnimation did not need to set a \c target
    value. As a convenience, when used in a transition, RotationAnimation will rotate all
    properties named "rotation" or "angle". You can override this by providing
    your own properties via \l {PropertyAnimation::properties}{properties} or
    \l {PropertyAnimation::property}{property}.

    Also, note the \l Rectangle will be rotated around its default
    \l {Item::}{transformOrigin} (which is \c Item.Center). To use a different
    transform origin, set the origin in the PropertyChanges object and apply
    the change at the start of the animation using PropertyAction. See the
    PropertyAction documentation for more details.

    Like any other animation type, a RotationAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {Animation and Transitions in Qt Quick} documentation shows a
    variety of methods for creating animations.

    \sa {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation}
*/
QVariant _q_interpolateShortestRotation(qreal &f, qreal &t, qreal progress)
{
    qreal newt = t;
    qreal diff = t-f;
    while(diff > 180.0){
        newt -= 360.0;
        diff -= 360.0;
    }
    while(diff < -180.0){
        newt += 360.0;
        diff += 360.0;
    }
    return QVariant(f + (newt - f) * progress);
}

QVariant _q_interpolateClockwiseRotation(qreal &f, qreal &t, qreal progress)
{
    qreal newt = t;
    qreal diff = t-f;
    while(diff < 0.0){
        newt += 360.0;
        diff += 360.0;
    }
    return QVariant(f + (newt - f) * progress);
}

QVariant _q_interpolateCounterclockwiseRotation(qreal &f, qreal &t, qreal progress)
{
    qreal newt = t;
    qreal diff = t-f;
    while(diff > 0.0){
        newt -= 360.0;
        diff -= 360.0;
    }
    return QVariant(f + (newt - f) * progress);
}

QQuickRotationAnimation::QQuickRotationAnimation(QObject *parent)
: QQuickPropertyAnimation(*(new QQuickRotationAnimationPrivate), parent)
{
    Q_D(QQuickRotationAnimation);
    d->interpolatorType = QMetaType::QReal;
    d->interpolator = QVariantAnimationPrivate::getInterpolator(d->interpolatorType);
    d->defaultProperties = QLatin1String("rotation,angle");
}

QQuickRotationAnimation::~QQuickRotationAnimation()
{
}

/*!
    \qmlproperty real QtQuick::RotationAnimation::from
    This property holds the starting value for the animation.

    For example, the following animation is not applied until the \c angle value
    has reached 100:

    \qml
    Item {
        states: [
            // ...
        ]

        transition: Transition {
            RotationAnimation { properties: "angle"; from: 100; duration: 2000 }
        }
    }
    \endqml

    If the RotationAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the starting state of the
    \l Transition, or the current value of the property at the moment the
    \l Behavior is triggered.

    \sa {Animation and Transitions in Qt Quick}
*/
qreal QQuickRotationAnimation::from() const
{
    Q_D(const QQuickRotationAnimation);
    return d->from.toReal();
}

void QQuickRotationAnimation::setFrom(qreal f)
{
    QQuickPropertyAnimation::setFrom(f);
}

/*!
    \qmlproperty real QtQuick::RotationAnimation::to
    This property holds the end value for the animation..

    If the RotationAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the end state of the
    \l Transition, or the value of the property change that triggered the
    \l Behavior.

    \sa {Animation and Transitions in Qt Quick}
*/
qreal QQuickRotationAnimation::to() const
{
    Q_D(const QQuickRotationAnimation);
    return d->to.toReal();
}

void QQuickRotationAnimation::setTo(qreal t)
{
    QQuickPropertyAnimation::setTo(t);
}

/*!
    \qmlproperty enumeration QtQuick::RotationAnimation::direction
    This property holds the direction of the rotation.

    Possible values are:

    \list
    \li RotationAnimation.Numerical (default) - Rotate by linearly interpolating between the two numbers.
           A rotation from 10 to 350 will rotate 340 degrees clockwise.
    \li RotationAnimation.Clockwise - Rotate clockwise between the two values
    \li RotationAnimation.Counterclockwise - Rotate counterclockwise between the two values
    \li RotationAnimation.Shortest - Rotate in the direction that produces the shortest animation path.
           A rotation from 10 to 350 will rotate 20 degrees counterclockwise.
    \endlist
*/
QQuickRotationAnimation::RotationDirection QQuickRotationAnimation::direction() const
{
    Q_D(const QQuickRotationAnimation);
    return d->direction;
}

void QQuickRotationAnimation::setDirection(QQuickRotationAnimation::RotationDirection direction)
{
    Q_D(QQuickRotationAnimation);
    if (d->direction == direction)
        return;

    d->direction = direction;
    switch(d->direction) {
    case Clockwise:
        d->interpolator = reinterpret_cast<QVariantAnimation::Interpolator>(&_q_interpolateClockwiseRotation);
        break;
    case Counterclockwise:
        d->interpolator = reinterpret_cast<QVariantAnimation::Interpolator>(&_q_interpolateCounterclockwiseRotation);
        break;
    case Shortest:
        d->interpolator = reinterpret_cast<QVariantAnimation::Interpolator>(&_q_interpolateShortestRotation);
        break;
    default:
        d->interpolator = QVariantAnimationPrivate::getInterpolator(d->interpolatorType);
        break;
    }
    emit directionChanged();
}



QQuickAnimationGroup::QQuickAnimationGroup(QObject *parent)
: QQuickAbstractAnimation(*(new QQuickAnimationGroupPrivate), parent)
{
}

QQuickAnimationGroup::QQuickAnimationGroup(QQuickAnimationGroupPrivate &dd, QObject *parent)
    : QQuickAbstractAnimation(dd, parent)
{
}

void QQuickAnimationGroupPrivate::append_animation(QQmlListProperty<QQuickAbstractAnimation> *list, QQuickAbstractAnimation *a)
{
    QQuickAnimationGroup *q = qmlobject_cast<QQuickAnimationGroup *>(list->object);
    if (q && a)
        a->setGroup(q);
}

void QQuickAnimationGroupPrivate::clear_animation(QQmlListProperty<QQuickAbstractAnimation> *list)
{
    QQuickAnimationGroup *q = qobject_cast<QQuickAnimationGroup *>(list->object);
    if (q) {
        while (q->d_func()->animations.count()) {
            QQuickAbstractAnimation *firstAnim = q->d_func()->animations.at(0);
            firstAnim->setGroup(0);
        }
    }
}

QQuickAnimationGroup::~QQuickAnimationGroup()
{
    Q_D(QQuickAnimationGroup);
    for (int i = 0; i < d->animations.count(); ++i)
        d->animations.at(i)->d_func()->group = 0;
    d->animations.clear();
}

QQmlListProperty<QQuickAbstractAnimation> QQuickAnimationGroup::animations()
{
    Q_D(QQuickAnimationGroup);
    QQmlListProperty<QQuickAbstractAnimation> list(this, d->animations);
    list.append = &QQuickAnimationGroupPrivate::append_animation;
    list.clear = &QQuickAnimationGroupPrivate::clear_animation;
    return list;
}

/*!
    \qmltype SequentialAnimation
    \instantiates QQuickSequentialAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-transitions-animations
    \inherits Animation
    \brief Allows animations to be run sequentially

    The SequentialAnimation and ParallelAnimation types allow multiple
    animations to be run together. Animations defined in a SequentialAnimation
    are run one after the other, while animations defined in a ParallelAnimation
    are run at the same time.

    The following example runs two number animations in a sequence.  The \l Rectangle
    animates to a \c x position of 50, then to a \c y position of 50.

    \snippet qml/sequentialanimation.qml 0

    Animations defined within a \l Transition are automatically run in parallel,
    so SequentialAnimation can be used to enclose the animations in a \l Transition
    if this is the preferred behavior.

    Like any other animation type, a SequentialAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {Animation and Transitions in Qt Quick} documentation shows a
    variety of methods for creating animations.

    \note Once an animation has been grouped into a SequentialAnimation or
    ParallelAnimation, it cannot be individually started and stopped; the
    SequentialAnimation or ParallelAnimation must be started and stopped as a group.

    \sa ParallelAnimation, {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation}
*/

QQuickSequentialAnimation::QQuickSequentialAnimation(QObject *parent) :
    QQuickAnimationGroup(parent)
{
}

QQuickSequentialAnimation::~QQuickSequentialAnimation()
{
}

QQuickAbstractAnimation::ThreadingModel QQuickSequentialAnimation::threadingModel() const
{
    Q_D(const QQuickAnimationGroup);

    ThreadingModel style = AnyThread;
    for (int i=0; i<d->animations.size(); ++i) {
        ThreadingModel ces = d->animations.at(i)->threadingModel();
        if (ces == GuiThread)
            return GuiThread;
        else if (ces == RenderThread)
            style = RenderThread;
    }
    return style;
}

QAbstractAnimationJob* QQuickSequentialAnimation::transition(QQuickStateActions &actions,
                                    QQmlProperties &modified,
                                    TransitionDirection direction,
                                    QObject *defaultTarget)
{
    Q_D(QQuickAnimationGroup);

    QSequentialAnimationGroupJob *ag = new QSequentialAnimationGroupJob;

    int inc = 1;
    int from = 0;
    if (direction == Backward) {
        inc = -1;
        from = d->animations.count() - 1;
    }

    ThreadingModel execution = threadingModel();

    bool valid = d->defaultProperty.isValid();
    QAbstractAnimationJob* anim;
    for (int ii = from; ii < d->animations.count() && ii >= 0; ii += inc) {
        if (valid)
            d->animations.at(ii)->setDefaultTarget(d->defaultProperty);
        anim = d->animations.at(ii)->transition(actions, modified, direction, defaultTarget);
        if (anim) {
            if (d->animations.at(ii)->threadingModel() == RenderThread && execution != RenderThread)
                anim = new QQuickAnimatorProxyJob(anim, this);
            inc == -1 ? ag->prependAnimation(anim) : ag->appendAnimation(anim);
        }
    }

    return initInstance(ag);
}



/*!
    \qmltype ParallelAnimation
    \instantiates QQuickParallelAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-transitions-animations
    \inherits Animation
    \brief Enables animations to be run in parallel

    The SequentialAnimation and ParallelAnimation types allow multiple
    animations to be run together. Animations defined in a SequentialAnimation
    are run one after the other, while animations defined in a ParallelAnimation
    are run at the same time.

    The following animation runs two number animations in parallel. The \l Rectangle
    moves to (50,50) by animating its \c x and \c y properties at the same time.

    \snippet qml/parallelanimation.qml 0

    Like any other animation type, a ParallelAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {Animation and Transitions in Qt Quick} documentation shows a
    variety of methods for creating animations.

    \note Once an animation has been grouped into a SequentialAnimation or
    ParallelAnimation, it cannot be individually started and stopped; the
    SequentialAnimation or ParallelAnimation must be started and stopped as a group.

    \sa SequentialAnimation, {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation}
*/
QQuickParallelAnimation::QQuickParallelAnimation(QObject *parent) :
    QQuickAnimationGroup(parent)
{
}

QQuickParallelAnimation::~QQuickParallelAnimation()
{
}

QQuickAbstractAnimation::ThreadingModel QQuickParallelAnimation::threadingModel() const
{
    Q_D(const QQuickAnimationGroup);

    ThreadingModel style = AnyThread;
    for (int i=0; i<d->animations.size(); ++i) {
        ThreadingModel ces = d->animations.at(i)->threadingModel();
        if (ces == GuiThread)
            return GuiThread;
        else if (ces == RenderThread)
            style = RenderThread;
    }
    return style;
}



QAbstractAnimationJob* QQuickParallelAnimation::transition(QQuickStateActions &actions,
                                      QQmlProperties &modified,
                                      TransitionDirection direction,
                                      QObject *defaultTarget)
{
    Q_D(QQuickAnimationGroup);
    QParallelAnimationGroupJob *ag = new QParallelAnimationGroupJob;

    ThreadingModel style = threadingModel();

    bool valid = d->defaultProperty.isValid();
    QAbstractAnimationJob* anim;
    for (int ii = 0; ii < d->animations.count(); ++ii) {
        if (valid)
            d->animations.at(ii)->setDefaultTarget(d->defaultProperty);
        anim = d->animations.at(ii)->transition(actions, modified, direction, defaultTarget);
        if (anim) {
            if (d->animations.at(ii)->threadingModel() == RenderThread && style != RenderThread)
                anim = new QQuickAnimatorProxyJob(anim, this);
            ag->appendAnimation(anim);
        }
    }
    return initInstance(ag);
}

//convert a variant from string type to another animatable type
void QQuickPropertyAnimationPrivate::convertVariant(QVariant &variant, int type)
{
    if (variant.userType() != QVariant::String) {
        variant.convert(type);
        return;
    }

    switch (type) {
    case QVariant::Rect:
    case QVariant::RectF:
    case QVariant::Point:
    case QVariant::PointF:
    case QVariant::Size:
    case QVariant::SizeF:
    case QVariant::Color:
    case QVariant::Vector3D:
        {
        bool ok = false;
        variant = QQmlStringConverters::variantFromString(variant.toString(), type, &ok);
        }
        break;
    default:
        if (QQmlValueTypeFactory::isValueType((uint)type)) {
            variant.convert(type);
        } else {
            QQmlMetaType::StringConverter converter = QQmlMetaType::customStringConverter(type);
            if (converter)
                variant = converter(variant.toString());
        }
        break;
    }
}

QQuickBulkValueAnimator::QQuickBulkValueAnimator()
    : QAbstractAnimationJob(), animValue(0), fromSourced(0), m_duration(250)
{
}

QQuickBulkValueAnimator::~QQuickBulkValueAnimator()
{
    delete animValue;
}

void QQuickBulkValueAnimator::setAnimValue(QQuickBulkValueUpdater *value)
{
    if (isRunning())
        stop();
    animValue = value;
}

void QQuickBulkValueAnimator::updateCurrentTime(int currentTime)
{
    if (isStopped())
        return;

    const qreal progress = easing.valueForProgress(((m_duration == 0) ? qreal(1) : qreal(currentTime) / qreal(m_duration)));

    if (animValue)
        animValue->setValue(progress);
}

void QQuickBulkValueAnimator::topLevelAnimationLoopChanged()
{
    //check for new from every top-level loop (when the top level animation is started and all subsequent loops)
    if (fromSourced)
        *fromSourced = false;
    QAbstractAnimationJob::topLevelAnimationLoopChanged();
}

void QQuickBulkValueAnimator::debugAnimation(QDebug d) const
{
    d << "BulkValueAnimation(" << hex << (const void *) this << dec << ")" << "duration:" << duration();

    if (animValue) {
        int indentLevel = 1;
        const QAbstractAnimationJob *job = this;
        while ((job = job->group()))
            ++indentLevel;
        animValue->debugUpdater(d, indentLevel);
    }
}

/*!
    \qmltype PropertyAnimation
    \instantiates QQuickPropertyAnimation
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-properties
    \inherits Animation
    \brief Animates changes in property values

    PropertyAnimation provides a way to animate changes to a property's value.

    It can be used to define animations in a number of ways:

    \list
    \li In a \l Transition

    For example, to animate any objects that have changed their \c x or \c y properties
    as a result of a state change, using an \c InOutQuad easing curve:

    \snippet qml/propertyanimation.qml transition


    \li In a \l Behavior

    For example, to animate all changes to a rectangle's \c x property:

    \snippet qml/propertyanimation.qml behavior


    \li As a property value source

    For example, to repeatedly animate the rectangle's \c x property:

    \snippet qml/propertyanimation.qml propertyvaluesource


    \li In a signal handler

    For example, to fade out \c theObject when clicked:
    \qml
    MouseArea {
        anchors.fill: theObject
        onClicked: PropertyAnimation { target: theObject; property: "opacity"; to: 0 }
    }
    \endqml

    \li Standalone

    For example, to animate \c rect's \c width property over 500ms, from its current width to 30:

    \snippet qml/propertyanimation.qml standalone

    \endlist

    Depending on how the animation is used, the set of properties normally used will be
    different. For more information see the individual property documentation, as well
    as the \l{Animation and Transitions in Qt Quick} introduction.

    Note that PropertyAnimation inherits the abstract \l Animation type.
    This includes additional properties and methods for controlling the animation.

    \sa {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation}
*/

QQuickPropertyAnimation::QQuickPropertyAnimation(QObject *parent)
: QQuickAbstractAnimation(*(new QQuickPropertyAnimationPrivate), parent)
{
}

QQuickPropertyAnimation::QQuickPropertyAnimation(QQuickPropertyAnimationPrivate &dd, QObject *parent)
: QQuickAbstractAnimation(dd, parent)
{
}

QQuickPropertyAnimation::~QQuickPropertyAnimation()
{
}

/*!
    \qmlproperty int QtQuick::PropertyAnimation::duration
    This property holds the duration of the animation, in milliseconds.

    The default value is 250.
*/
int QQuickPropertyAnimation::duration() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->duration;
}

void QQuickPropertyAnimation::setDuration(int duration)
{
    if (duration < 0) {
        qmlInfo(this) << tr("Cannot set a duration of < 0");
        return;
    }

    Q_D(QQuickPropertyAnimation);
    if (d->duration == duration)
        return;
    d->duration = duration;
    emit durationChanged(duration);
}

/*!
    \qmlproperty variant QtQuick::PropertyAnimation::from
    This property holds the starting value for the animation.

    If the PropertyAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the starting state of the
    \l Transition, or the current value of the property at the moment the
    \l Behavior is triggered.

    \sa {Animation and Transitions in Qt Quick}
*/
QVariant QQuickPropertyAnimation::from() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->from;
}

void QQuickPropertyAnimation::setFrom(const QVariant &f)
{
    Q_D(QQuickPropertyAnimation);
    if (d->fromIsDefined && f == d->from)
        return;
    d->from = f;
    d->fromIsDefined = f.isValid();
    emit fromChanged(f);
}

/*!
    \qmlproperty variant QtQuick::PropertyAnimation::to
    This property holds the end value for the animation.

    If the PropertyAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the end state of the
    \l Transition, or the value of the property change that triggered the
    \l Behavior.

    \sa {Animation and Transitions in Qt Quick}
*/
QVariant QQuickPropertyAnimation::to() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->to;
}

void QQuickPropertyAnimation::setTo(const QVariant &t)
{
    Q_D(QQuickPropertyAnimation);
    if (d->toIsDefined && t == d->to)
        return;
    d->to = t;
    d->toIsDefined = t.isValid();
    emit toChanged(t);
}

/*!
    \qmlpropertygroup QtQuick::PropertyAnimation::easing
    \qmlproperty enumeration QtQuick::PropertyAnimation::easing.type
    \qmlproperty real QtQuick::PropertyAnimation::easing.amplitude
    \qmlproperty real QtQuick::PropertyAnimation::easing.overshoot
    \qmlproperty real QtQuick::PropertyAnimation::easing.period
    \qmlproperty list<real> QtQuick::PropertyAnimation::easing.bezierCurve

//! propertyanimation.easing
    \brief Specifies the easing curve used for the animation

    To specify an easing curve you need to specify at least the type. For some curves you can also specify
    amplitude, period and/or overshoot (more details provided after the table). The default easing curve is
    \c Easing.Linear.

    \qml
    PropertyAnimation { properties: "y";
                        easing.type: Easing.InOutElastic;
                        easing.amplitude: 2.0;
                        easing.period: 1.5 }
    \endqml

    Available types are:

    \table
    \row
        \li \c Easing.Linear
        \li Easing curve for a linear (t) function: velocity is constant.
        \li \inlineimage qeasingcurve-linear.png
    \row
        \li \c Easing.InQuad
        \li Easing curve for a quadratic (t^2) function: accelerating from zero velocity.
        \li \inlineimage qeasingcurve-inquad.png
    \row
        \li \c Easing.OutQuad
        \li Easing curve for a quadratic (t^2) function: decelerating to zero velocity.
        \li \inlineimage qeasingcurve-outquad.png
    \row
        \li \c Easing.InOutQuad
        \li Easing curve for a quadratic (t^2) function: acceleration until halfway, then deceleration.
        \li \inlineimage qeasingcurve-inoutquad.png
    \row
        \li \c Easing.OutInQuad
        \li Easing curve for a quadratic (t^2) function: deceleration until halfway, then acceleration.
        \li \inlineimage qeasingcurve-outinquad.png
    \row
        \li \c Easing.InCubic
        \li Easing curve for a cubic (t^3) function: accelerating from zero velocity.
        \li \inlineimage qeasingcurve-incubic.png
    \row
        \li \c Easing.OutCubic
        \li Easing curve for a cubic (t^3) function: decelerating to zero velocity.
        \li \inlineimage qeasingcurve-outcubic.png
    \row
        \li \c Easing.InOutCubic
        \li Easing curve for a cubic (t^3) function: acceleration until halfway, then deceleration.
        \li \inlineimage qeasingcurve-inoutcubic.png
    \row
        \li \c Easing.OutInCubic
        \li Easing curve for a cubic (t^3) function: deceleration until halfway, then acceleration.
        \li \inlineimage qeasingcurve-outincubic.png
    \row
        \li \c Easing.InQuart
        \li Easing curve for a quartic (t^4) function: accelerating from zero velocity.
        \li \inlineimage qeasingcurve-inquart.png
    \row
        \li \c Easing.OutQuart
        \li Easing curve for a quartic (t^4) function: decelerating to zero velocity.
        \li \inlineimage qeasingcurve-outquart.png
    \row
        \li \c Easing.InOutQuart
        \li Easing curve for a quartic (t^4) function: acceleration until halfway, then deceleration.
        \li \inlineimage qeasingcurve-inoutquart.png
    \row
        \li \c Easing.OutInQuart
        \li Easing curve for a quartic (t^4) function: deceleration until halfway, then acceleration.
        \li \inlineimage qeasingcurve-outinquart.png
    \row
        \li \c Easing.InQuint
        \li Easing curve for a quintic (t^5) function: accelerating from zero velocity.
        \li \inlineimage qeasingcurve-inquint.png
    \row
        \li \c Easing.OutQuint
        \li Easing curve for a quintic (t^5) function: decelerating to zero velocity.
        \li \inlineimage qeasingcurve-outquint.png
    \row
        \li \c Easing.InOutQuint
        \li Easing curve for a quintic (t^5) function: acceleration until halfway, then deceleration.
        \li \inlineimage qeasingcurve-inoutquint.png
    \row
        \li \c Easing.OutInQuint
        \li Easing curve for a quintic (t^5) function: deceleration until halfway, then acceleration.
        \li \inlineimage qeasingcurve-outinquint.png
    \row
        \li \c Easing.InSine
        \li Easing curve for a sinusoidal (sin(t)) function: accelerating from zero velocity.
        \li \inlineimage qeasingcurve-insine.png
    \row
        \li \c Easing.OutSine
        \li Easing curve for a sinusoidal (sin(t)) function: decelerating to zero velocity.
        \li \inlineimage qeasingcurve-outsine.png
    \row
        \li \c Easing.InOutSine
        \li Easing curve for a sinusoidal (sin(t)) function: acceleration until halfway, then deceleration.
        \li \inlineimage qeasingcurve-inoutsine.png
    \row
        \li \c Easing.OutInSine
        \li Easing curve for a sinusoidal (sin(t)) function: deceleration until halfway, then acceleration.
        \li \inlineimage qeasingcurve-outinsine.png
    \row
        \li \c Easing.InExpo
        \li Easing curve for an exponential (2^t) function: accelerating from zero velocity.
        \li \inlineimage qeasingcurve-inexpo.png
    \row
        \li \c Easing.OutExpo
        \li Easing curve for an exponential (2^t) function: decelerating to zero velocity.
        \li \inlineimage qeasingcurve-outexpo.png
    \row
        \li \c Easing.InOutExpo
        \li Easing curve for an exponential (2^t) function: acceleration until halfway, then deceleration.
        \li \inlineimage qeasingcurve-inoutexpo.png
    \row
        \li \c Easing.OutInExpo
        \li Easing curve for an exponential (2^t) function: deceleration until halfway, then acceleration.
        \li \inlineimage qeasingcurve-outinexpo.png
    \row
        \li \c Easing.InCirc
        \li Easing curve for a circular (sqrt(1-t^2)) function: accelerating from zero velocity.
        \li \inlineimage qeasingcurve-incirc.png
    \row
        \li \c Easing.OutCirc
        \li Easing curve for a circular (sqrt(1-t^2)) function: decelerating to zero velocity.
        \li \inlineimage qeasingcurve-outcirc.png
    \row
        \li \c Easing.InOutCirc
        \li Easing curve for a circular (sqrt(1-t^2)) function: acceleration until halfway, then deceleration.
        \li \inlineimage qeasingcurve-inoutcirc.png
    \row
        \li \c Easing.OutInCirc
        \li Easing curve for a circular (sqrt(1-t^2)) function: deceleration until halfway, then acceleration.
        \li \inlineimage qeasingcurve-outincirc.png
    \row
        \li \c Easing.InElastic
        \li Easing curve for an elastic (exponentially decaying sine wave) function: accelerating from zero velocity.
        \br The peak amplitude can be set with the \e amplitude parameter, and the period of decay by the \e period parameter.
        \li \inlineimage qeasingcurve-inelastic.png
    \row
        \li \c Easing.OutElastic
        \li Easing curve for an elastic (exponentially decaying sine wave) function: decelerating to zero velocity.
        \br The peak amplitude can be set with the \e amplitude parameter, and the period of decay by the \e period parameter.
        \li \inlineimage qeasingcurve-outelastic.png
    \row
        \li \c Easing.InOutElastic
        \li Easing curve for an elastic (exponentially decaying sine wave) function: acceleration until halfway, then deceleration.
        \li \inlineimage qeasingcurve-inoutelastic.png
    \row
        \li \c Easing.OutInElastic
        \li Easing curve for an elastic (exponentially decaying sine wave) function: deceleration until halfway, then acceleration.
        \li \inlineimage qeasingcurve-outinelastic.png
    \row
        \li \c Easing.InBack
        \li Easing curve for a back (overshooting cubic function: (s+1)*t^3 - s*t^2) easing in: accelerating from zero velocity.
        \li \inlineimage qeasingcurve-inback.png
    \row
        \li \c Easing.OutBack
        \li Easing curve for a back (overshooting cubic function: (s+1)*t^3 - s*t^2) easing out: decelerating to zero velocity.
        \li \inlineimage qeasingcurve-outback.png
    \row
        \li \c Easing.InOutBack
        \li Easing curve for a back (overshooting cubic function: (s+1)*t^3 - s*t^2) easing in/out: acceleration until halfway, then deceleration.
        \li \inlineimage qeasingcurve-inoutback.png
    \row
        \li \c Easing.OutInBack
        \li Easing curve for a back (overshooting cubic easing: (s+1)*t^3 - s*t^2) easing out/in: deceleration until halfway, then acceleration.
        \li \inlineimage qeasingcurve-outinback.png
    \row
        \li \c Easing.InBounce
        \li Easing curve for a bounce (exponentially decaying parabolic bounce) function: accelerating from zero velocity.
        \li \inlineimage qeasingcurve-inbounce.png
    \row
        \li \c Easing.OutBounce
        \li Easing curve for a bounce (exponentially decaying parabolic bounce) function: decelerating to zero velocity.
        \li \inlineimage qeasingcurve-outbounce.png
    \row
        \li \c Easing.InOutBounce
        \li Easing curve for a bounce (exponentially decaying parabolic bounce) function easing in/out: acceleration until halfway, then deceleration.
        \li \inlineimage qeasingcurve-inoutbounce.png
    \row
        \li \c Easing.OutInBounce
        \li Easing curve for a bounce (exponentially decaying parabolic bounce) function easing out/in: deceleration until halfway, then acceleration.
        \li \inlineimage qeasingcurve-outinbounce.png
    \row
        \li \c Easing.Bezier
        \li Custom easing curve defined by the easing.bezierCurve property.
        \li
    \endtable

    \c easing.amplitude is only applicable for bounce and elastic curves (curves of type
    \c Easing.InBounce, \c Easing.OutBounce, \c Easing.InOutBounce, \c Easing.OutInBounce, \c Easing.InElastic,
    \c Easing.OutElastic, \c Easing.InOutElastic or \c Easing.OutInElastic).

    \c easing.overshoot is only applicable if \c easing.type is: \c Easing.InBack, \c Easing.OutBack,
    \c Easing.InOutBack or \c Easing.OutInBack.

    \c easing.period is only applicable if easing.type is: \c Easing.InElastic, \c Easing.OutElastic,
    \c Easing.InOutElastic or \c Easing.OutInElastic.

    \c easing.bezierCurve is only applicable if easing.type is: \c Easing.Bezier.  This property is a list<real> containing
    groups of three points defining a curve from 0,0 to 1,1 - control1, control2,
    end point: [cx1, cy1, cx2, cy2, endx, endy, ...].  The last point must be 1,1.

    See the \l {Qt Quick Examples - Animation#Easing Curves}{Easing Curves} for a demonstration of the different easing settings.
//! propertyanimation.easing
*/
QEasingCurve QQuickPropertyAnimation::easing() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->easing;
}

void QQuickPropertyAnimation::setEasing(const QEasingCurve &e)
{
    Q_D(QQuickPropertyAnimation);
    if (d->easing == e)
        return;

    d->easing = e;
    emit easingChanged(e);
}

QObject *QQuickPropertyAnimation::target() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->target;
}

void QQuickPropertyAnimation::setTargetObject(QObject *o)
{
    Q_D(QQuickPropertyAnimation);
    if (d->target == o)
        return;
    d->target = o;
    emit targetChanged();
}

QString QQuickPropertyAnimation::property() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->propertyName;
}

void QQuickPropertyAnimation::setProperty(const QString &n)
{
    Q_D(QQuickPropertyAnimation);
    if (d->propertyName == n)
        return;
    d->propertyName = n;
    emit propertyChanged();
}

QString QQuickPropertyAnimation::properties() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->properties;
}

void QQuickPropertyAnimation::setProperties(const QString &prop)
{
    Q_D(QQuickPropertyAnimation);
    if (d->properties == prop)
        return;

    d->properties = prop;
    emit propertiesChanged(prop);
}

/*!
    \qmlproperty string QtQuick::PropertyAnimation::properties
    \qmlproperty list<Object> QtQuick::PropertyAnimation::targets
    \qmlproperty string QtQuick::PropertyAnimation::property
    \qmlproperty Object QtQuick::PropertyAnimation::target

    These properties are used as a set to determine which properties should be animated.
    The singular and plural forms are functionally identical, e.g.
    \qml
    NumberAnimation { target: theItem; property: "x"; to: 500 }
    \endqml
    has the same meaning as
    \qml
    NumberAnimation { targets: theItem; properties: "x"; to: 500 }
    \endqml
    The singular forms are slightly optimized, so if you do have only a single target/property
    to animate you should try to use them.

    The \c targets property allows multiple targets to be set. For example, this animates the
    \c x property of both \c itemA and \c itemB:

    \qml
    NumberAnimation { targets: [itemA, itemB]; properties: "x"; to: 500 }
    \endqml

    In many cases these properties do not need to be explicitly specified, as they can be
    inferred from the animation framework:

    \table 80%
    \row
    \li Value Source / Behavior
    \li When an animation is used as a value source or in a Behavior, the default target and property
       name to be animated can both be inferred.
       \qml
       Rectangle {
           id: theRect
           width: 100; height: 100
           color: Qt.rgba(0,0,1)
           NumberAnimation on x { to: 500; loops: Animation.Infinite } //animate theRect's x property
           Behavior on y { NumberAnimation {} } //animate theRect's y property
       }
       \endqml
    \row
    \li Transition
    \li When used in a transition, a property animation is assumed to match \e all targets
       but \e no properties. In practice, that means you need to specify at least the properties
       in order for the animation to do anything.
       \qml
       Rectangle {
           id: theRect
           width: 100; height: 100
           color: Qt.rgba(0,0,1)
           Item { id: uselessItem }
           states: State {
               name: "state1"
               PropertyChanges { target: theRect; x: 200; y: 200; z: 4 }
               PropertyChanges { target: uselessItem; x: 10; y: 10; z: 2 }
           }
           transitions: Transition {
               //animate both theRect's and uselessItem's x and y to their final values
               NumberAnimation { properties: "x,y" }

               //animate theRect's z to its final value
               NumberAnimation { target: theRect; property: "z" }
           }
       }
       \endqml
    \row
    \li Standalone
    \li When an animation is used standalone, both the target and property need to be
       explicitly specified.
       \qml
       Rectangle {
           id: theRect
           width: 100; height: 100
           color: Qt.rgba(0,0,1)
           //need to explicitly specify target and property
           NumberAnimation { id: theAnim; target: theRect; property: "x"; to: 500 }
           MouseArea {
               anchors.fill: parent
               onClicked: theAnim.start()
           }
       }
       \endqml
    \endtable

    As seen in the above example, properties is specified as a comma-separated string of property names to animate.

    \sa exclude, {Animation and Transitions in Qt Quick}
*/
QQmlListProperty<QObject> QQuickPropertyAnimation::targets()
{
    Q_D(QQuickPropertyAnimation);
    return QQmlListProperty<QObject>(this, d->targets);
}

/*!
    \qmlproperty list<Object> QtQuick::PropertyAnimation::exclude
    This property holds the items not to be affected by this animation.
    \sa PropertyAnimation::targets
*/
QQmlListProperty<QObject> QQuickPropertyAnimation::exclude()
{
    Q_D(QQuickPropertyAnimation);
    return QQmlListProperty<QObject>(this, d->exclude);
}

void QQuickAnimationPropertyUpdater::setValue(qreal v)
{
    bool deleted = false;
    wasDeleted = &deleted;
    if (reverse)
        v = 1 - v;
    for (int ii = 0; ii < actions.count(); ++ii) {
        QQuickStateAction &action = actions[ii];

        if (v == 1.) {
            QQmlPropertyPrivate::write(action.property, action.toValue, QQmlPropertyData::BypassInterceptor | QQmlPropertyData::DontRemoveBinding);
        } else {
            if (!fromSourced && !fromDefined) {
                action.fromValue = action.property.read();
                if (interpolatorType) {
                    QQuickPropertyAnimationPrivate::convertVariant(action.fromValue, interpolatorType);
                }
            }
            if (!interpolatorType) {
                int propType = action.property.propertyType();
                if (!prevInterpolatorType || prevInterpolatorType != propType) {
                    prevInterpolatorType = propType;
                    interpolator = QVariantAnimationPrivate::getInterpolator(prevInterpolatorType);
                }
            }
            if (interpolator)
                QQmlPropertyPrivate::write(action.property, interpolator(action.fromValue.constData(), action.toValue.constData(), v), QQmlPropertyData::BypassInterceptor | QQmlPropertyData::DontRemoveBinding);
        }
        if (deleted)
            return;
    }
    wasDeleted = 0;
    fromSourced = true;
}

void QQuickAnimationPropertyUpdater::debugUpdater(QDebug d, int indentLevel) const
{
    QByteArray ind(indentLevel, ' ');
    for (int i = 0; i < actions.count(); ++i) {
        const QQuickStateAction &action = actions.at(i);
        d << "\n" << ind.constData() << "target:" << action.property.object() << "property:" << action.property.name()
          << "from:" << action.fromValue << "to:" << action.toValue;
    }
}

QQuickStateActions QQuickPropertyAnimation::createTransitionActions(QQuickStateActions &actions,
                                                                                QQmlProperties &modified,
                                                                                QObject *defaultTarget)
{
    Q_D(QQuickPropertyAnimation);
    QQuickStateActions newActions;

    QStringList props = d->properties.isEmpty() ? QStringList() : d->properties.split(QLatin1Char(','));
    for (int ii = 0; ii < props.count(); ++ii)
        props[ii] = props.at(ii).trimmed();
    if (!d->propertyName.isEmpty())
        props << d->propertyName;

    QList<QObject*> targets = d->targets;
    if (d->target)
        targets.append(d->target);

    bool hasSelectors = !props.isEmpty() || !targets.isEmpty() || !d->exclude.isEmpty();
    bool useType = (props.isEmpty() && d->defaultToInterpolatorType) ? true : false;

    if (d->defaultProperty.isValid() && !hasSelectors) {
        props << d->defaultProperty.name();
        targets << d->defaultProperty.object();
    }

    if (defaultTarget && targets.isEmpty())
        targets << defaultTarget;

    bool usingDefaultProperties = false;
    if (props.isEmpty() && !d->defaultProperties.isEmpty()) {
        props << d->defaultProperties.split(QLatin1Char(','));
        usingDefaultProperties = true;
    }

    bool hasExplicit = false;
    //an explicit animation has been specified
    if (d->toIsDefined) {
        QVector<QString> errorMessages;
        bool successfullyCreatedDefaultProperty = false;

        for (int i = 0; i < props.count(); ++i) {
            for (int j = 0; j < targets.count(); ++j) {
                QQuickStateAction myAction;
                QString errorMessage;
                const QString propertyName = props.at(i);
                myAction.property = d->createProperty(targets.at(j), propertyName, this, &errorMessage);
                if (myAction.property.isValid()) {
                    if (usingDefaultProperties)
                        successfullyCreatedDefaultProperty = true;

                    if (d->fromIsDefined) {
                        myAction.fromValue = d->from;
                        d->convertVariant(myAction.fromValue, d->interpolatorType ? d->interpolatorType : myAction.property.propertyType());
                    }
                    myAction.toValue = d->to;
                    d->convertVariant(myAction.toValue, d->interpolatorType ? d->interpolatorType : myAction.property.propertyType());
                    newActions << myAction;
                    hasExplicit = true;
                    for (int ii = 0; ii < actions.count(); ++ii) {
                        QQuickStateAction &action = actions[ii];
                        if (action.property.object() == myAction.property.object() &&
                            myAction.property.name() == action.property.name()) {
                            modified << action.property;
                            break;  //### any chance there could be multiples?
                        }
                    }
                } else {
                    errorMessages.append(errorMessage);
                }
            }
        }

        if (!successfullyCreatedDefaultProperty) {
            for (const QString &errorMessage : qAsConst(errorMessages))
                qmlInfo(this) << errorMessage;
        }
    }

    if (!hasExplicit)
    for (int ii = 0; ii < actions.count(); ++ii) {
        QQuickStateAction &action = actions[ii];

        QObject *obj = action.property.object();
        QString propertyName = action.property.name();
        QObject *sObj = action.specifiedObject;
        QString sPropertyName = action.specifiedProperty;
        bool same = (obj == sObj);

        if ((targets.isEmpty() || targets.contains(obj) || (!same && targets.contains(sObj))) &&
           (!d->exclude.contains(obj)) && (same || (!d->exclude.contains(sObj))) &&
           (props.contains(propertyName) || (!same && props.contains(sPropertyName))
               || (useType && action.property.propertyType() == d->interpolatorType))) {
            QQuickStateAction myAction = action;

            if (d->fromIsDefined)
                myAction.fromValue = d->from;
            else
                myAction.fromValue = QVariant();
            if (d->toIsDefined)
                myAction.toValue = d->to;

            d->convertVariant(myAction.fromValue, d->interpolatorType ? d->interpolatorType : myAction.property.propertyType());
            d->convertVariant(myAction.toValue, d->interpolatorType ? d->interpolatorType : myAction.property.propertyType());

            modified << action.property;

            newActions << myAction;
            action.fromValue = myAction.toValue;
        }
    }
    return newActions;
}

QAbstractAnimationJob* QQuickPropertyAnimation::transition(QQuickStateActions &actions,
                                                                     QQmlProperties &modified,
                                                                     TransitionDirection direction,
                                                                     QObject *defaultTarget)
{
    Q_D(QQuickPropertyAnimation);

    QQuickStateActions dataActions = createTransitionActions(actions, modified, defaultTarget);

    QQuickBulkValueAnimator *animator = new QQuickBulkValueAnimator;
    animator->setDuration(d->duration);
    animator->setEasingCurve(d->easing);

    if (!dataActions.isEmpty()) {
        QQuickAnimationPropertyUpdater *data = new QQuickAnimationPropertyUpdater;
        data->interpolatorType = d->interpolatorType;
        data->interpolator = d->interpolator;
        data->reverse = direction == Backward ? true : false;
        data->fromSourced = false;
        data->fromDefined = d->fromIsDefined;
        data->actions = dataActions;
        animator->setAnimValue(data);
        animator->setFromSourcedValue(&data->fromSourced);
        d->actions = &data->actions; //remove this?
    }

    return initInstance(animator);
}

QQuickAnimationPropertyUpdater::~QQuickAnimationPropertyUpdater()
{
    if (wasDeleted) *wasDeleted = true;
}

QT_END_NAMESPACE
