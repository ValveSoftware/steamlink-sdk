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

#include "inputsequence_p.h"
#include <Qt3DInput/qinputsequence.h>
#include <Qt3DInput/qabstractphysicaldevice.h>
#include <Qt3DInput/private/qinputsequence_p.h>
#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <QDateTime>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

namespace Input {

InputSequence::InputSequence()
    : AbstractActionInput()
    , m_sequences()
    , m_inputsToTrigger()
    , m_timeout(0)
    , m_buttonInterval(0)
    , m_startTime(0)
    , m_lastInputTime(0)
{
}

void InputSequence::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QInputSequenceData>>(change);
    const QInputSequenceData &data = typedChange->data;
    m_sequences = data.sequenceIds;
    m_timeout = milliToNano(data.timeout);
    m_buttonInterval = milliToNano(data.buttonInterval);
    m_inputsToTrigger = m_sequences;
}

void InputSequence::cleanup()
{
    setEnabled(false);
    m_timeout = 0;
    m_buttonInterval = 0;
    m_startTime = 0;
    m_lastInputTime = 0;
    m_lastInputId = Qt3DCore::QNodeId();
    m_sequences.clear();
    m_inputsToTrigger.clear();
}

void InputSequence::setStartTime(qint64 time)
{
    m_startTime = time;
}

void InputSequence::reset()
{
    m_startTime = 0;
    m_lastInputTime = 0;
    m_inputsToTrigger = m_sequences;
    m_lastInputId = Qt3DCore::QNodeId();
}

bool InputSequence::actionTriggered(Qt3DCore::QNodeId input, const qint64 currentTime)
{
    if (input != m_inputsToTrigger.first())
        return false;

    // Save the last input
    m_lastInputId = input;
    // Return false if we've spent too much time in between two sequences
    if ((m_lastInputTime != 0) && ((currentTime - m_lastInputTime) > m_buttonInterval)) {
        reset();
        return false;
    }

    // Update lastInputTime and remove the inputs to trigger from the sequence
    m_lastInputTime = currentTime;
    m_inputsToTrigger.removeOne(input);

    // If we have no more remaining inputs in the sequences of inputs
    if (m_inputsToTrigger.isEmpty()) {
        // All Triggered
        reset();
        return true;
    }
    return false;
}

void InputSequence::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {
    case Qt3DCore::PropertyUpdated: {
        const auto change = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("timeout")) {
            m_timeout = milliToNano(change->value().toInt());
        } else if (change->propertyName() == QByteArrayLiteral("buttonInterval")) {
            m_buttonInterval = milliToNano(change->value().toInt());
        }
        break;
    }

    case Qt3DCore::PropertyValueAdded: {
        const auto change = qSharedPointerCast<Qt3DCore::QPropertyNodeAddedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("sequence")) {
            m_sequences.push_back(change->addedNodeId());
            m_inputsToTrigger.push_back(change->addedNodeId());
        }
        break;
    }

    case Qt3DCore::PropertyValueRemoved: {
        const auto change = qSharedPointerCast<Qt3DCore::QPropertyNodeRemovedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("sequence")) {
            m_sequences.removeOne(change->removedNodeId());
            m_inputsToTrigger.removeOne(change->removedNodeId());
        }
        break;
    }

    default:
        break;
    }
    AbstractActionInput::sceneChangeEvent(e);
}

bool InputSequence::process(InputHandler *inputHandler, qint64 currentTime)
{
    if (!isEnabled())
        return false;

    if (m_startTime != 0) {
        // Check if we are still inside the time limit for the sequence
        if ((currentTime - m_startTime) > m_timeout) {
            reset();
            return false;
        }
    }

    bool triggered = false;
    for (const Qt3DCore::QNodeId &actionInputId : qAsConst(m_sequences)) {
        AbstractActionInput *actionInput = inputHandler->lookupActionInput(actionInputId);
        if (actionInput && actionInput->process(inputHandler, currentTime)) {
            triggered |= actionTriggered(actionInputId, currentTime);
            if (m_startTime == 0)
                m_startTime = currentTime;
        }
    }
    return triggered;
}

} // namespace Input

} // namespace Qt3DInput

QT_END_NAMESPACE

