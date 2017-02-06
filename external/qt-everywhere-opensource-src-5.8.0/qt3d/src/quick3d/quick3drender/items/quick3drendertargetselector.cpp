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

#include "quick3drendertargetselector_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {
namespace Quick {

Quick3DRenderTargetSelector::Quick3DRenderTargetSelector(QObject *parent)
    : QObject(parent)
{
}

Quick3DRenderTargetSelector::~Quick3DRenderTargetSelector()
{
}

QVariantList Quick3DRenderTargetSelector::drawBuffers() const
{
    // Converts RenderAttachmentType to int
    QVariantList l;
    const auto outputs = parentRenderTargetSelector()->outputs();
    l.reserve(outputs.size());
    for (const QRenderTargetOutput::AttachmentPoint &b : outputs)
        l.append(static_cast<int>(b));
    return l;
}

void Quick3DRenderTargetSelector::setDrawBuffers(const QVariantList &buffers)
{
    if (buffers != drawBuffers()) {

        // Converts int to RenderAttachmentType
        QVector<QRenderTargetOutput::AttachmentPoint> drawBuffersList;
        drawBuffersList.reserve(buffers.size());
        for (const QVariant &buf : buffers)
            drawBuffersList.append(static_cast<QRenderTargetOutput::AttachmentPoint>(buf.toInt()));

        parentRenderTargetSelector()->setOutputs(drawBuffersList);
        emit drawBuffersChanged();
    }
}

} // namespace Quick
} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
