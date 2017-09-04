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

#include <Qt3DQuickRender/private/quick3drendertargetoutput_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {
namespace Quick {

Quick3DRenderTargetOutput::Quick3DRenderTargetOutput(QObject * parent)
    : QObject(parent)
{
}

QQmlListProperty<QRenderTargetOutput> Quick3DRenderTargetOutput::qmlAttachments()
{
    return QQmlListProperty<QRenderTargetOutput>(this, 0,
                                               &Quick3DRenderTargetOutput::appendRenderAttachment,
                                               &Quick3DRenderTargetOutput::renderAttachmentCount,
                                               &Quick3DRenderTargetOutput::renderAttachmentAt,
                                               &Quick3DRenderTargetOutput::clearRenderAttachments);
}

void Quick3DRenderTargetOutput::appendRenderAttachment(QQmlListProperty<QRenderTargetOutput> *list, QRenderTargetOutput *output)
{
    Quick3DRenderTargetOutput *rT = qobject_cast<Quick3DRenderTargetOutput *>(list->object);
    if (rT)
        rT->parentRenderTarget()->addOutput(output);
}

QRenderTargetOutput *Quick3DRenderTargetOutput::renderAttachmentAt(QQmlListProperty<QRenderTargetOutput> *list, int index)
{
    Quick3DRenderTargetOutput *rT = qobject_cast<Quick3DRenderTargetOutput *>(list->object);
    if (rT)
        return rT->parentRenderTarget()->outputs().at(index);
    return nullptr;
}

int Quick3DRenderTargetOutput::renderAttachmentCount(QQmlListProperty<QRenderTargetOutput> *list)
{
    Quick3DRenderTargetOutput *rT = qobject_cast<Quick3DRenderTargetOutput *>(list->object);
    if (rT)
        return rT->parentRenderTarget()->outputs().count();
    return -1;
}

void Quick3DRenderTargetOutput::clearRenderAttachments(QQmlListProperty<QRenderTargetOutput> *list)
{
    Quick3DRenderTargetOutput *rT = qobject_cast<Quick3DRenderTargetOutput *>(list->object);
    if (rT) {
        const auto outputs = rT->parentRenderTarget()->outputs();
        for (QRenderTargetOutput *output : outputs)
            rT->parentRenderTarget()->removeOutput(output);
    }
}

} // namespace Quick
} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
