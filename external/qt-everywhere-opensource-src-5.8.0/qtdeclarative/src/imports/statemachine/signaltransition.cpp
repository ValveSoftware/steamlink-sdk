/****************************************************************************
**
** Copyright (C) 2016 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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
#include <private/qqmlcontext_p.h>
#include <private/qqmlboundsignal_p.h>

SignalTransition::SignalTransition(QState *parent)
    : QSignalTransition(this, SIGNAL(invokeYourself()), parent), m_complete(false), m_signalExpression(Q_NULLPTR)
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

    QQmlContext *outerContext = QQmlEngine::contextForObject(this);
    QQmlContext context(outerContext);
    QQmlContextData::get(outerContext)->imports->addref();
    QQmlContextData::get(&context)->imports = QQmlContextData::get(outerContext)->imports;

    QStateMachine::SignalEvent *e = static_cast<QStateMachine::SignalEvent*>(event);

    // Set arguments as context properties
    int count = e->arguments().count();
    QMetaMethod metaMethod = e->sender()->metaObject()->method(e->signalIndex());
    const auto parameterNames = metaMethod.parameterNames();
    for (int i = 0; i < count; i++)
        context.setContextProperty(parameterNames[i], QVariant::fromValue(e->arguments().at(i)));

    QQmlExpression expr(m_guard, &context, this);
    QVariant result = expr.evaluate();

    return result.toBool();
}

void SignalTransition::onTransition(QEvent *event)
{
    if (m_signalExpression) {
        QStateMachine::SignalEvent *e = static_cast<QStateMachine::SignalEvent*>(event);
        m_signalExpression->evaluate(e->arguments());
    }
    QSignalTransition::onTransition(event);
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

    QObject *sender;
    QMetaMethod signalMethod;

    QV4::ScopedValue value(scope, QJSValuePrivate::convertedToValue(jsEngine, m_signal));

    // Did we get the "slot" that can be used to invoke the signal?
    if (QV4::QObjectMethod *signalSlot = value->as<QV4::QObjectMethod>()) {
        sender = signalSlot->object();
        Q_ASSERT(sender);
        signalMethod = sender->metaObject()->method(signalSlot->methodIndex());
    } else if (QV4::QmlSignalHandler *signalObject = value->as<QV4::QmlSignalHandler>()) { // or did we get the signal object (the one with the connect()/disconnect() functions) ?
        sender = signalObject->object();
        Q_ASSERT(sender);
        signalMethod = sender->metaObject()->method(signalObject->signalIndex());
    } else {
        qmlInfo(this) << tr("Specified signal does not exist.");
        return;
    }

    QSignalTransition::setSenderObject(sender);
    QSignalTransition::setSignal(signalMethod.methodSignature());

    connectTriggered();
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

void SignalTransition::connectTriggered()
{
    if (!m_complete || !m_compilationUnit)
        return;

    QObject *target = senderObject();
    QQmlData *ddata = QQmlData::get(this);
    QQmlContextData *ctxtdata = ddata ? ddata->outerContext : 0;

    Q_ASSERT(m_bindings.count() == 1);
    const QV4::CompiledData::Binding *binding = m_bindings.at(0);
    Q_ASSERT(binding->type == QV4::CompiledData::Binding::Type_Script);

    QV4::ExecutionEngine *jsEngine = QV8Engine::getV4(QQmlEngine::contextForObject(this)->engine());
    QV4::Scope scope(jsEngine);
    QV4::Scoped<QV4::QObjectMethod> qobjectSignal(scope, QJSValuePrivate::convertedToValue(jsEngine, m_signal));
    Q_ASSERT(qobjectSignal);
    QMetaMethod metaMethod = target->metaObject()->method(qobjectSignal->methodIndex());
    int signalIndex = QMetaObjectPrivate::signalIndex(metaMethod);

    QQmlBoundSignalExpression *expression = ctxtdata ?
                new QQmlBoundSignalExpression(target, signalIndex,
                                              ctxtdata, this, m_compilationUnit->runtimeFunctions[binding->value.compiledScriptIndex]) : 0;
    if (expression)
        expression->setNotifyOnValueChanged(false);
    m_signalExpression = expression;
}

void SignalTransitionParser::verifyBindings(const QV4::CompiledData::Unit *qmlUnit, const QList<const QV4::CompiledData::Binding *> &props)
{
    for (int ii = 0; ii < props.count(); ++ii) {
        const QV4::CompiledData::Binding *binding = props.at(ii);

        QString propName = qmlUnit->stringAt(binding->propertyNameIndex);

        if (propName != QLatin1String("onTriggered")) {
            error(props.at(ii), SignalTransition::tr("Cannot assign to non-existent property \"%1\"").arg(propName));
            return;
        }

        if (binding->type != QV4::CompiledData::Binding::Type_Script) {
            error(binding, SignalTransition::tr("SignalTransition: script expected"));
            return;
        }
    }
}

void SignalTransitionParser::applyBindings(QObject *object, QV4::CompiledData::CompilationUnit *compilationUnit, const QList<const QV4::CompiledData::Binding *> &bindings)
{
    SignalTransition *st = qobject_cast<SignalTransition*>(object);
    st->m_compilationUnit = compilationUnit;
    st->m_bindings = bindings;
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
