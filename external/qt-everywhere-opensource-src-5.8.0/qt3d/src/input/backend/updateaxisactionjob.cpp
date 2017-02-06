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

#include "updateaxisactionjob_p.h"
#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DInput/private/inputmanagers_p.h>
#include <Qt3DInput/private/job_common_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

namespace Input {

UpdateAxisActionJob::UpdateAxisActionJob(qint64 currentTime, InputHandler *handler, HLogicalDevice handle)
    : Qt3DCore::QAspectJob()
    , m_currentTime(currentTime)
    , m_handler(handler)
    , m_handle(handle)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::UpdateAxisAction, 0);
}

void UpdateAxisActionJob::run()
{
    // Note: we assume axis/action are not really shared:
    // there's no benefit in sharing those when it comes to computing
    LogicalDevice *device = m_handler->logicalDeviceManager()->data(m_handle);

    if (!device->isEnabled())
        return;

    updateAction(device);
    updateAxis(device);
}

void UpdateAxisActionJob::updateAction(LogicalDevice *device)
{
    const auto actionIds = device->actions();
    for (const Qt3DCore::QNodeId actionId : actionIds) {
        bool actionTriggered = false;
        Action *action = m_handler->actionManager()->lookupResource(actionId);

        const auto actionInputIds = action->inputs();
        for (const Qt3DCore::QNodeId actionInputId : actionInputIds)
            actionTriggered |= processActionInput(actionInputId);

        action->setActionTriggered(actionTriggered);
    }
}

bool UpdateAxisActionJob::processActionInput(const Qt3DCore::QNodeId actionInputId)
{
    AbstractActionInput *actionInput = m_handler->lookupActionInput(actionInputId);
    Q_ASSERT(actionInput);
    return actionInput->process(m_handler, m_currentTime);
}

void UpdateAxisActionJob::updateAxis(LogicalDevice *device)
{
    const auto axisIds = device->axes();
    for (const Qt3DCore::QNodeId axisId : axisIds) {
        Axis *axis = m_handler->axisManager()->lookupResource(axisId);
        float axisValue = 0.0f;

        const auto axisInputIds = axis->inputs();
        for (const Qt3DCore::QNodeId axisInputId : axisInputIds)
            axisValue += processAxisInput(axisInputId);

        // Clamp the axisValue -1/1
        axisValue = qMin(1.0f, qMax(axisValue, -1.0f));
        axis->setAxisValue(axisValue);
    }
}

float UpdateAxisActionJob::processAxisInput(const Qt3DCore::QNodeId axisInputId)
{
    AnalogAxisInput *analogInput = m_handler->analogAxisInputManager()->lookupResource(axisInputId);
    if (analogInput)
        return analogInput->process(m_handler, m_currentTime);

    ButtonAxisInput *buttonInput = m_handler->buttonAxisInputManager()->lookupResource(axisInputId);
    if (buttonInput)
        return buttonInput->process(m_handler, m_currentTime);

    Q_UNREACHABLE();
    return 0.0f;
}

} // Input

} // Qt3DInput

QT_END_NAMESPACE
