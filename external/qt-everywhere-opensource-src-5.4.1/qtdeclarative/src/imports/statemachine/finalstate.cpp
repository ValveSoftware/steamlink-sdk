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
