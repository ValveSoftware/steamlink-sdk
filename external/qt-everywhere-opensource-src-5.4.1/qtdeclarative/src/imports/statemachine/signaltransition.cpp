/****************************************************************************
**
** Copyright (C) 2014 Ford Motor Company
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "signaltransition.h"

#include <QStateMachine>
#include <QMetaProperty>
#include <QQmlInfo>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlExpression>

#include <private/qv4qobjectwrapper_p.h>
#include <private/qv8engine_p.h>
#include <private/qjsvalue_p.h>
#include <private/qv4scopedvalue_p.h>

SignalTransition::SignalTransition(QState *parent)
    : QSignalTransition(this, SIGNAL(invokeYourself()), parent)
{
    connect(this, SIGNAL(signalChanged()), SIGNAL(qmlSignalChanged()));
}

bool SignalTransition::eventTest(QEvent *event)
{
    Q_ASSERT(event);
    if (!QSignalTransition::eventTest(event))
        return false;

    if (m_guard.isEmpty())
        return true;

    QQmlContext context(QQmlEngine::contextForObject(this));

    QStateMachine::SignalEvent *e = static_cast<QStateMachine::SignalEvent*>(event);

    // Set arguments as context properties
    int count = e->arguments().count();
    QMetaMethod metaMethod = e->sender()->metaObject()->method(e->signalIndex());
    for (int i = 0; i < count; i++)
        context.setContextProperty(metaMethod.parameterNames()[i], QVariant::fromValue(e->arguments().at(i)));

    QQmlExpression expr(m_guard, &context, this);
    QVariant result = expr.evaluate();

    return result.toBool();
}

const QJSValue& SignalTransition::signal()
{
    return m_signal;
}

void SignalTransition::setSignal(const QJSValue &signal)
{
    if (m_signal.strictlyEquals(signal))
        return;

    m_signal = signal;

    QV4::ExecutionEngine *jsEngine = QV8Engine::getV4(QQmlEngine::contextForObject(this)->engine());
    QV4::Scope scope(jsEngine);

    QV4::Scoped<QV4::QObjectMethod> qobjectSignal(scope, QJSValuePrivate::get(m_signal)->getValue(jsEngine));
    Q_ASSERT(qobjectSignal);

    QObject *sender = qobjectSignal->object();
    Q_ASSERT(sender);
    QMetaMethod metaMethod = sender->metaObject()->method(qobjectSignal->methodIndex());

    QSignalTransition::setSenderObject(sender);
    QSignalTransition::setSignal(metaMethod.methodSignature());
}

QQmlScriptString SignalTransition::guard() const
{
    return m_guard;
}

void SignalTransition::setGuard(const QQmlScriptString &guard)
{
    if (m_guard == guard)
        return;

    m_guard = guard;
    emit guardChanged();
}

void SignalTransition::invoke()
{
    emit invokeYourself();
}

/*!
    \qmltype QAbstractTransition
    \inqmlmodule QtQml.StateMachine
    \omit
    \ingroup statemachine-qmltypes
    \endomit
    \since 5.4

    \brief The QAbstractTransition type is the base type of transitions between QAbstractState objects.

    The QAbstractTransition type is the abstract base type of transitions
    between states (QAbstractState objects) of a StateMachine.
    QAbstractTransition is part of \l{The Declarative State Machine Framework}.

    The sourceState() property has the source of the transition. The
    targetState and targetStates properties return the target(s) of the
    transition.

    The triggered() signal is emitted when the transition has been triggered.

    Do not use QAbstractTransition directly; use SignalTransition or
    TimeoutTransition instead.

    \sa SignalTransition, TimeoutTransition
*/

/*!
    \qmlproperty bool QAbstractTransition::sourceState
    \readonly sourceState

    \brief The source state (parent) of this transition.
*/

/*!
    \qmlproperty QAbstractState QAbstractTransition::targetState

    \brief The target state of this transition.

    If a transition has no target state, the transition may still be
    triggered, but this will not cause the state machine's configuration to
    change (i.e. the current state will not be exited and re-entered).
*/

/*!
    \qmlproperty list<QAbstractState> QAbstractTransition::targetStates

    \brief The target states of this transition.

    If multiple states are specified, they all must be descendants of the
    same parallel group state.
*/

/*!
    \qmlsignal QAbstractTransition::triggered()

    This signal is emitted when the transition has been triggered.

    The corresponding handler is \c onTriggered.
*/

/*!
    \qmltype QSignalTransition
    \inqmlmodule QtQml.StateMachine
    \inherits QAbstractTransition
    \omit
    \ingroup statemachine-qmltypes
    \endomit
    \since 5.4

    \brief The QSignalTransition type provides a transition based on a Qt signal.

    Do not use QSignalTransition directly; use SignalTransition or
    TimeoutTransition instead.

    \sa SignalTransition, TimeoutTransition
*/

/*!
    \qmlproperty string QSignalTransition::signal

    \brief The signal which is associated with this signal transition.
*/

/*!
    \qmlproperty QObject QSignalTransition::senderObject

    \brief The sender object which is associated with this signal transition.
*/


/*!
    \qmltype SignalTransition
    \inqmlmodule QtQml.StateMachine
    \inherits QSignalTransition
    \ingroup statemachine-qmltypes
    \since 5.4

    \brief The SignalTransition type provides a transition based on a Qt signal.

    SignalTransition is part of \l{The Declarative State Machine Framework}.

    \section1 Example Usage

    \snippet qml/statemachine/signaltransition.qml document

    \clearfloat

    \sa StateMachine, FinalState, TimeoutTransition
*/

/*!
    \qmlproperty signal SignalTransition::signal

    \brief The signal which is associated with this signal transition.

    \snippet qml/statemachine/signaltransitionsignal.qml document
*/

/*!
    \qmlproperty bool SignalTransition::guard

    Guard conditions affect the behavior of a state machine by enabling
    transitions only when they evaluate to true and disabling them when
    they evaluate to false.

    When the signal associated with this signal transition is emitted the
    guard condition is evaluated. In the guard condition the arguments
    of the signal can be used as demonstrated in the example below.

    \snippet qml/statemachine/guardcondition.qml document

    \sa signal
*/
