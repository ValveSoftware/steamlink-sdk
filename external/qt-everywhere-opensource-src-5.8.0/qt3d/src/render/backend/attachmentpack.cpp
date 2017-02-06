/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include "attachmentpack_p.h"
#include <Qt3DRender/private/rendertarget_p.h>
#include <Qt3DRender/private/rendertargetselectornode_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

AttachmentPack::AttachmentPack()
{
}

AttachmentPack::AttachmentPack(const RenderTargetSelector *selector, const RenderTarget *target, AttachmentManager *attachmentManager)
{
    // Cache draw buffers list
    const QVector<QRenderTargetOutput::AttachmentPoint> selectedAttachmentPoints = selector->outputs();

    // Copy attachments
    const auto outputIds = target->renderOutputs();
    for (Qt3DCore::QNodeId outputId : outputIds) {
        const RenderTargetOutput *output = attachmentManager->lookupResource(outputId);
        if (output)
            m_attachments.append(output->attachment());
    }

    // Create actual DrawBuffers list that is used for glDrawBuffers

    // If nothing is specified, use all the attachments as draw buffers
    if (selectedAttachmentPoints.isEmpty()) {
        for (const Attachment &attachment : qAsConst(m_attachments))
            // only consider Color Attachments
            if (attachment.m_point <= QRenderTargetOutput::Color15)
                m_drawBuffers.push_back((int) attachment.m_point);
    } else {
        for (QRenderTargetOutput::AttachmentPoint drawBuffer : selectedAttachmentPoints)
            if (drawBuffer <= QRenderTargetOutput::Color15)
                m_drawBuffers.push_back((int) drawBuffer);
    }
}

// return index of given attachment within actual draw buffers list
int AttachmentPack::getDrawBufferIndex(QRenderTargetOutput::AttachmentPoint attachmentPoint) const
{
    for (int i = 0; i < m_drawBuffers.size(); i++)
        if (m_drawBuffers.at(i) == (int)attachmentPoint)
            return i;
    return -1;
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
