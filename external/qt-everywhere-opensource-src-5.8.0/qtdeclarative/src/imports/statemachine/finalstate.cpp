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

#include "finalstate.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlInfo>

FinalState::FinalState(QState *parent)
    : QFinalState(parent)
{
}

QQmlListProperty<QObject> FinalState::children()
{
    return QQmlListProperty<QObject>(this, &m_children, m_children.appendNoTransition, m_children.count, m_children.at, m_children.clear);
}

/*!
    \qmltype FinalState
    \inqmlmodule QtQml.StateMachine
    \inherits QAbstractState
    \ingroup statemachine-qmltypes
    \since 5.4

    \brief Provides a final state.


    A final state is used to communicate that (part of) a StateMachine has
    finished its work.  When a final top-level state is entered, the state
    machine's \l{State::finished}{finished}() signal is emitted. In
    general, when a final substate (a child of a State) is entered, the parent
    state's \l{State::finished}{finished}() signal is emitted.  FinalState
    is part of \l{The Declarative State Machine Framework}.

    To use a final state, you create a FinalState object and add a transition
    to it from another state.

    \section1 Example Usage

    \snippet qml/statemachine/finalstate.qml document

    \clearfloat

    \sa StateMachine, State
*/
