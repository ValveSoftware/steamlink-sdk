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

#include "state.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlInfo>

State::State(QState *parent)
    : QState(parent)
{
}

void State::componentComplete()
{
    if (this->machine() == NULL) {
        static bool once = false;
        if (!once) {
            once = true;
            qmlInfo(this) << "No top level StateMachine found.  Nothing will run without a StateMachine.";
        }
    }
}

QQmlListProperty<QObject> State::children()
{
    return QQmlListProperty<QObject>(this, &m_children, m_children.append, m_children.count, m_children.at, m_children.clear);
}

/*!
    \qmltype QAbstractState
    \inqmlmodule QtQml.StateMachine
    \omit
    \ingroup statemachine-qmltypes
    \endomit
    \since 5.4

    \brief The QAbstractState type is the base type of States of a StateMachine.

    Do not use QAbstractState directly; use State, FinalState or
    StateMachine instead.

    \sa StateMachine, State
*/

/*!
    \qmlproperty bool QAbstractState::active
    \readonly active

    The active property of this state. A state is active between
    entered() and exited() signals. This property is readonly.

    \sa entered, exited
*/

/*!
    \qmlsignal QAbstractState::entered()

    This signal is emitted when the State becomes active.

    The corresponding handler is \c onEntered.

    \sa active, exited
*/

/*!
    \qmlsignal QAbstractState::exited()

    This signal is emitted when the State becomes inactive.

    The corresponding handler is \c onExited.

    \sa active, entered
*/

/*!
    \qmltype State
    \inqmlmodule QtQml.StateMachine
    \inherits QAbstractState
    \ingroup statemachine-qmltypes
    \since 5.4

    \brief Provides a general-purpose state for StateMachine.


    State objects can have child states as well as transitions to other
    states. State is part of \l{The Declarative State Machine Framework}.

    \section1 States with Child States

    The childMode property determines how child states are treated.  For
    non-parallel state groups, the initialState property must be used to
    set the initial state.  The child states are mutually exclusive states,
    and the state machine needs to know which child state to enter when the
    parent state is the target of a transition.

    The state emits the State::finished() signal when a final child state
    (FinalState) is entered.

    The errorState sets the state's error state.  The error state is the state
    that the state machine will transition to if an error is detected when
    attempting to enter the state (e.g. because no initial state has been set).

    \section1 Example Usage

    \snippet qml/statemachine/basicstate.qml document

    \clearfloat

    \sa StateMachine, FinalState
*/

/*!
    \qmlproperty enumeration State::childMode

    \brief The child mode of this state

    The default value of this property is QState.ExclusiveStates.

    This enum specifies how a state's child states are treated:
    \list
    \li QState.ExclusiveStates The child states are mutually exclusive and an initial state must be set by setting initialState property.
    \li QState.ParallelStates The child states are parallel. When the parent state is entered, all its child states are entered in parallel.
    \endlist
*/

/*!
    \qmlproperty QAbstractState State::errorState

    \brief The error state of this state.
*/

/*!
    \qmlproperty QAbstractState State::initialState

    \brief The initial state of this state (one of its child states).
*/

/*!
    \qmlsignal State::finished()

    This signal is emitted when a final child state of this state is entered.

    The corresponding handler is \c onFinished.

    \sa QAbstractState::active, QAbstractState::entered, QAbstractState::exited
*/

/*!
    \qmltype HistoryState
    \inqmlmodule QtQml.StateMachine
    \inherits QAbstractState
    \ingroup statemachine-qmltypes
    \since 5.4

    \brief The HistoryState type provides a means of returning to a previously active substate.

    A history state is a pseudo-state that represents the child state that the
    parent state was in the last time the parent state was exited.  A transition
    with a history state as its target is in fact a transition to one of the
    other child states of the parent state.
    HistoryState is part of \l{The Declarative State Machine Framework}.

    Use the defaultState property to set the state that should be entered
    if the parent state has never been entered.

    \section1 Example Usage

    \snippet qml/statemachine/historystate.qml document

    \clearfloat

    By default, a history state is shallow, meaning that it will not remember
    nested states.  This can be configured through the historyType property.

    \sa StateMachine, State
*/

/*!
    \qmlproperty QAbstractState HistoryState::defaultState

    \brief The default state of this history state.

    The default state indicates the state to transition to if the parent
    state has never been entered before.
*/

/*!
    \qmlproperty enumeration HistoryState::historyType

    \brief The type of history that this history state records.

    The default value of this property is HistoryState.ShallowHistory.

    This enum specifies the type of history that a HistoryState records.
    \list
    \li HistoryState.ShallowHistory Only the immediate child states of the
        parent state are recorded.  In this case, a transition with the history
        state as its target will end up in the immediate child state that the
        parent was in the last time it was exited.  This is the default.
    \li HistoryState.DeepHistory Nested states are recorded.  In this case
        a transition with the history state as its target will end up in the
        most deeply nested descendant state the parent was in the last time
        it was exited.
    \endlist
*/

