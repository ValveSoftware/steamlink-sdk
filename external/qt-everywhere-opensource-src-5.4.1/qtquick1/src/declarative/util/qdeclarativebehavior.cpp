/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "private/qdeclarativebehavior_p.h"

#include "private/qdeclarativeanimation_p.h"
#include "private/qdeclarativetransition_p.h"

#include <qdeclarativecontext.h>
#include <qdeclarativeinfo.h>
#include <private/qdeclarativeproperty_p.h>
#include <private/qdeclarativeguard_p.h>
#include <private/qdeclarativeengine_p.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QDeclarativeBehaviorPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QDeclarativeBehavior)
public:
    QDeclarativeBehaviorPrivate() : animation(0), enabled(true), finalized(false)
      , blockRunningChanged(false) {}

    QDeclarativeProperty property;
    QVariant currentValue;
    QVariant targetValue;
    QDeclarativeGuard<QDeclarativeAbstractAnimation> animation;
    bool enabled;
    bool finalized;
    bool blockRunningChanged;
};

/*!
    \qmltype Behavior
    \instantiates QDeclarativeBehavior
    \ingroup qml-animation-transition
    \since 4.7
    \brief The Behavior element allows you to specify a default animation for a property change.

    A Behavior defines the default animation to be applied whenever a
    particular property value changes.

    For example, the following Behavior defines a NumberAnimation to be run
    whenever the \l Rectangle's \c width value changes. When the MouseArea
    is clicked, the \c width is changed, triggering the behavior's animation:

    \snippet doc/src/snippets/declarative/behavior.qml 0

    Note that a property cannot have more than one assigned Behavior. To provide
    multiple animations within a Behavior, use ParallelAnimation or
    SequentialAnimation.

    If a \l{QML States}{state change} has a \l Transition that matches the same property as a
    Behavior, the \l Transition animation overrides the Behavior for that
    state change. For general advice on using Behaviors to animate state changes, see
    \l{Using QML Behaviors with States}.

    \sa {QML Animation and Transitions}, {declarative/animation/behaviors}{Behavior example}, QtDeclarative
*/


QDeclarativeBehavior::QDeclarativeBehavior(QObject *parent)
    : QObject(*(new QDeclarativeBehaviorPrivate), parent)
{
}

QDeclarativeBehavior::~QDeclarativeBehavior()
{
}

/*!
    \qmlproperty Animation Behavior::animation
    \default

    This property holds the animation to run when the behavior is triggered.
*/

QDeclarativeAbstractAnimation *QDeclarativeBehavior::animation()
{
    Q_D(QDeclarativeBehavior);
    return d->animation;
}

void QDeclarativeBehavior::setAnimation(QDeclarativeAbstractAnimation *animation)
{
    Q_D(QDeclarativeBehavior);
    if (d->animation) {
        qmlInfo(this) << tr("Cannot change the animation assigned to a Behavior.");
        return;
    }

    d->animation = animation;
    if (d->animation) {
        d->animation->setDefaultTarget(d->property);
        d->animation->setDisableUserControl();
        connect(d->animation->qtAnimation(),
                SIGNAL(stateChanged(QAbstractAnimation::State,QAbstractAnimation::State)),
                this,
                SLOT(qtAnimationStateChanged(QAbstractAnimation::State,QAbstractAnimation::State)));
    }
}


void QDeclarativeBehavior::qtAnimationStateChanged(QAbstractAnimation::State newState,QAbstractAnimation::State)
{
    Q_D(QDeclarativeBehavior);
    if (!d->blockRunningChanged)
        d->animation->notifyRunningChanged(newState == QAbstractAnimation::Running);
}


/*!
    \qmlproperty bool Behavior::enabled

    This property holds whether the behavior will be triggered when the tracked
    property changes value.

    By default a Behavior is enabled.
*/

bool QDeclarativeBehavior::enabled() const
{
    Q_D(const QDeclarativeBehavior);
    return d->enabled;
}

void QDeclarativeBehavior::setEnabled(bool enabled)
{
    Q_D(QDeclarativeBehavior);
    if (d->enabled == enabled)
        return;
    d->enabled = enabled;
    emit enabledChanged();
}

void QDeclarativeBehavior::write(const QVariant &value)
{
    Q_D(QDeclarativeBehavior);
    qmlExecuteDeferred(this);
    if (!d->animation || !d->enabled || !d->finalized) {
        QDeclarativePropertyPrivate::write(d->property, value, QDeclarativePropertyPrivate::BypassInterceptor | QDeclarativePropertyPrivate::DontRemoveBinding);
        d->targetValue = value;
        return;
    }

    if (d->animation->isRunning() && value == d->targetValue)
        return;

    d->currentValue = d->property.read();
    d->targetValue = value;

    if (d->animation->qtAnimation()->duration() != -1
            && d->animation->qtAnimation()->state() != QAbstractAnimation::Stopped) {
        d->blockRunningChanged = true;
        d->animation->qtAnimation()->stop();
    }

    QDeclarativeStateOperation::ActionList actions;
    QDeclarativeAction action;
    action.property = d->property;
    action.fromValue = d->currentValue;
    action.toValue = value;
    actions << action;

    QList<QDeclarativeProperty> after;
    d->animation->transition(actions, after, QDeclarativeAbstractAnimation::Forward);
    d->animation->qtAnimation()->start();
    d->blockRunningChanged = false;
    if (!after.contains(d->property))
        QDeclarativePropertyPrivate::write(d->property, value, QDeclarativePropertyPrivate::BypassInterceptor | QDeclarativePropertyPrivate::DontRemoveBinding);
}

void QDeclarativeBehavior::setTarget(const QDeclarativeProperty &property)
{
    Q_D(QDeclarativeBehavior);
    d->property = property;
    d->currentValue = property.read();
    if (d->animation)
        d->animation->setDefaultTarget(property);

    QDeclarativeEnginePrivate *engPriv = QDeclarativeEnginePrivate::get(qmlEngine(this));
    engPriv->registerFinalizedParserStatusObject(this, this->metaObject()->indexOfSlot("componentFinalized()"));
}

void QDeclarativeBehavior::componentFinalized()
{
    Q_D(QDeclarativeBehavior);
    d->finalized = true;
}

QT_END_NAMESPACE
