/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include "mouseeventdispatcherjob_p.h"
#include "inputhandler_p.h"
#include "mousehandler_p.h"
#include "inputmanagers_p.h"
#include "job_common_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DInput {
namespace Input {

MouseEventDispatcherJob::MouseEventDispatcherJob(Qt3DCore::QNodeId input,
                                                 const QList<QT_PREPEND_NAMESPACE (QMouseEvent)> &mouseEvents,
                                                 const QList<QT_PREPEND_NAMESPACE (QWheelEvent)> &wheelEvents)
    : QAspectJob()
    , m_inputHandler(nullptr)
    , m_mouseInput(input)
    , m_mouseEvents(mouseEvents)
    , m_wheelEvents(wheelEvents)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::MouseEventDispatcher, 0);
}

void MouseEventDispatcherJob::setInputHandler(InputHandler *handler)
{
    m_inputHandler = handler;
}

void MouseEventDispatcherJob::run()
{
    MouseHandler *input = m_inputHandler->mouseInputManager()->lookupResource(m_mouseInput);
    if (input) {
        // Send mouse and wheel events to frontend
        for (const QT_PREPEND_NAMESPACE(QMouseEvent) &e : m_mouseEvents)
            input->mouseEvent(QMouseEventPtr(new QMouseEvent(e)));
        for (const QT_PREPEND_NAMESPACE(QWheelEvent) &e : m_wheelEvents)
            input->wheelEvent(QWheelEventPtr(new QWheelEvent(e)));
    }
}

} // namespace Input
} // namespace Qt3DInput

QT_END_NAMESPACE
