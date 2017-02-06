/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "inputchord_p.h"
#include <Qt3DInput/qinputchord.h>
#include <Qt3DInput/private/qinputchord_p.h>
#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

namespace Input {

InputChord::InputChord()
    : AbstractActionInput()
    , m_chords()
    , m_inputsToTrigger()
    , m_timeout(0)
    , m_startTime(0)
{
}

void InputChord::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QInputChordData>>(change);
    const QInputChordData &data = typedChange->data;
    m_chords = data.chordIds;
    m_timeout = milliToNano(data.timeout);
    m_inputsToTrigger = m_chords;
}

void InputChord::cleanup()
{
    setEnabled(false);
    m_timeout = 0;
    m_startTime = 0;
    m_chords.clear();
    m_inputsToTrigger.clear();
}

void InputChord::reset()
{
    m_startTime = 0;
    m_inputsToTrigger = m_chords;
}

bool InputChord::actionTriggered(Qt3DCore::QNodeId input)
{
    m_inputsToTrigger.removeOne(input);
    if (m_inputsToTrigger.isEmpty()) {
        //All Triggered
        reset();
        return true;
    }
    return false;
}

void InputChord::setStartTime(qint64 time)
{
    m_startTime = time;
}

void InputChord::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {
    case Qt3DCore::PropertyUpdated: {
        const auto change = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("timeout"))
            m_timeout = milliToNano(change->value().toInt());
        break;
    }

    case Qt3DCore::PropertyValueAdded: {
        const auto change = qSharedPointerCast<Qt3DCore::QPropertyNodeAddedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("chord")) {
            m_chords.push_back(change->addedNodeId());
            m_inputsToTrigger.push_back(change->addedNodeId());
        }
        break;
    }

    case Qt3DCore::PropertyValueRemoved: {
        const auto change = qSharedPointerCast<Qt3DCore::QPropertyNodeRemovedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("chord")) {
            m_chords.removeOne(change->removedNodeId());
            m_inputsToTrigger.removeOne(change->removedNodeId());
        }
        break;
    }

    default:
        break;
    }
    AbstractActionInput::sceneChangeEvent(e);
}

bool InputChord::process(InputHandler *inputHandler, qint64 currentTime)
{
    if (!isEnabled())
        return false;

    const qint64 startTime = m_startTime;
    bool triggered = false;
    int activeInputs = 0;
    for (const Qt3DCore::QNodeId &actionInputId : qAsConst(m_chords)) {
        AbstractActionInput *actionInput = inputHandler->lookupActionInput(actionInputId);
        if (actionInput && actionInput->process(inputHandler, currentTime)) {
            triggered |= actionTriggered(actionInputId);
            activeInputs++;
            if (startTime == 0)
                m_startTime = currentTime;
        }
    }

    if (startTime != 0) {
        // Check if we are still inside the time limit for the chord
        if ((currentTime - startTime) > m_timeout) {
            reset();
            if (activeInputs > 0)
                m_startTime = startTime;
            return false;
        }
    }

    return triggered;
}

} // namespace Input

} // namespace Qt3DInput

QT_END_NAMESPACE

