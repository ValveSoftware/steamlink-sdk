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

#include "statemachine.h"

#include <QAbstractTransition>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlInfo>

#include <private/qqmlopenmetaobject_p.h>
#include <private/qqmlengine_p.h>

StateMachine::StateMachine(QObject *parent)
    : QStateMachine(parent), m_completed(false), m_running(false)
{
    connect(this, SIGNAL(runningChanged(bool)), SIGNAL(qmlRunningChanged()));
}

bool StateMachine::isRunning() const
{
    return QStateMachine::isRunning();
}

void StateMachine::setRunning(bool running)
{
    if (m_completed)
        QStateMachine::setRunning(running);
    else
        m_running = running;
}

void StateMachine::componentComplete()
{
    if (QStateMachine::initialState() == NULL && childMode() == QState::ExclusiveStates)
         qmlInfo(this) << "No initial state set for StateMachine";

    // Everything is proper setup, now start the state-machine if we got
    // asked to do so.
    m_completed = true;
    if (m_running)
        setRunning(true);
}

QQmlListProperty<QObject> StateMachine::children()
{
    return QQmlListProperty<QObject>(this, &m_children, m_children.append, m_children.count, m_children.at, m_children.clear);
}

/*!
    \qmltype StateMachine
    \inqmlmodule QtQml.StateMachine
    \inherits State
    \ingroup statemachine-qmltypes
    \since 5.4

    \brief Provides a hierarchical finite state machine.

    StateMachine is based on the concepts and notation of
    \l{http://www.wisdom.weizmann.ac.il/~dharel/SCANNED.PAPERS/Statecharts.pdf}{Statecharts}.
    StateMachine is part of \l{The Declarative State Machine Framework}.

    A state machine manages a set of states and transitions between those
    states; these states and transitions define a state graph.  Once a state
    graph has been built, the state machine can execute it. StateMachine's
    execution algorithm is based on the \l{http://www.w3.org/TR/scxml/}{State Chart XML (SCXML)}
    algorithm. The framework's \l{The Declarative State Machine Framework}{overview}
    gives several state graphs and the code to build them.

    Before the machine can be started, the \l{State::initialState}{initialState}
    must be set. The initial state is the state that the
    machine enters when started.  You can then set running property to true
    or start() the state machine.  The started signal is emitted when the
    initial state is entered.

    The state machine processes events and takes transitions until a
    top-level final state is entered; the state machine then emits the
    finished() signal. You can also stop() the state machine
    explicitly (you can also set running property to false).
    The stopped signal is emitted in this case.

    \section1 Example Usage
    The following snippet shows a state machine that will finish when a button
    is clicked:

    \snippet qml/statemachine/simplestatemachine.qml document

    If an error is encountered, the machine will look for an
    \l{State::errorState}{errorState}, and if one is available, it will
    enter this state.  After the error state is entered, the type of the error
    can be retrieved with error().  The execution of the state graph will not
    stop when the error state is entered.  If no error state applies to the
    erroneous state, the machine will stop executing and an error message will
    be printed to the console.

    \clearfloat

    \sa QAbstractState, State, SignalTransition, TimeoutTransition, HistoryState {The Declarative State Machine Framework}
*/

/*!
    \qmlproperty enumeration StateMachine::globalRestorePolicy

    \brief The restore policy for states of this state machine.

    The default value of this property is QState.DontRestoreProperties.

    This enum specifies the restore policy type.  The restore policy
    takes effect when the machine enters a state which sets one or more
    properties.  If the restore policy is set to QState.RestoreProperties,
    the state machine will save the original value of the property before the
    new value is set.

    Later, when the machine either enters a state which does not set a
    value for the given property, the property will automatically be restored
    to its initial value.

    Only one initial value will be saved for any given property.  If a value
    for a property has already been saved by the state machine, it will not be
    overwritten until the property has been successfully restored.

    \list
    \li QState.DontRestoreProperties The state machine should not save the initial values of properties and restore them later.
    \li QState.RestoreProperties The state machine should save the initial values of properties and restore them later.
    \endlist
*/

/*!
    \qmlproperty bool StateMachine::running

    \brief The running state of this state machine.
    \sa start(), stop()
*/

/*!
    \qmlproperty string StateMachine::errorString
    \readonly errorString

    \brief The error string of this state machine.
*/


/*!
    \qmlmethod StateMachine::start()

    Starts this state machine.  The machine will reset its configuration and
    transition to the initial state.  When a final top-level state (FinalState)
    is entered, the machine will emit the finished() signal.

    \note A state machine will not run without a running event loop, such as
    the main application event loop started with QCoreApplication::exec() or
    QApplication::exec().

    \sa started, State::finished, stop(), State::initialState, running
*/

/*!
    \qmlsignal StateMachine::started()

    This signal is emitted when the state machine has entered its initial state
    (State::initialState).

    The corresponding handler is \c onStarted.

    \sa running, start(), State::finished
*/

/*!
    \qmlmethod StateMachine::stop()

    Stops this state machine.  The state machine will stop processing events
    and then emit the stopped signal.

    \sa stopped, start(), running
*/

/*!
    \qmlsignal StateMachine::stopped()

    This signal is emitted when the state machine has stopped.

    The corresponding handler is \c onStopped.

    \sa running, stop(), State::finished
*/
