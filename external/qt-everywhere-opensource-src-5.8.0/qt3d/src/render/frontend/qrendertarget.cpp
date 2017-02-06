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

#include "qrendertarget.h"
#include "qrendertarget_p.h"
#include "qrendertargetoutput.h"
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {

/*!
    \class Qt3DRender::QRenderTarget
    \brief The QRenderTarget class encapsulates a target (usually a frame buffer
    object) which the renderer can render into.
    \since 5.7
    \inmodule Qt3DRender

    A Qt3DRender::QRenderTarget comprises of Qt3DRender::QRenderTargetOutput objects,
    which specify the the buffers the render target is rendering to. The user can
    specify MRT(Multiple Render Targets) by attaching multiple textures to different
    attachment points. The results are undefined if the user tries to attach multiple
    textures to the same attachment point. At render time, only the draw buffers specified
    in the Qt3DRender::QRenderTargetSelector are used.

 */
/*!
    \qmltype RenderTarget
    \brief The RenderTarget class encapsulates a target (usually a frame buffer
    object) which the renderer can render into.
    \since 5.7
    \inmodule Qt3D.Render
    \instantiates Qt3DRender::QRenderTarget

    A RenderTarget comprises of RenderTargetOutput objects, which specify the the buffers
    the render target is rendering to. The user can specify MRT(Multiple Render Targets)
    by attaching multiple textures to different attachment points. The results are undefined
    if the user tries to attach multiple textures to the same attachment point. At render
    time, only the draw buffers specified in the RenderTargetSelector are used.
 */

/*!
    \qmlproperty list<RenderTargetOutput> RenderTarget::attachments
    Holds the attachments for the RenderTarget.
*/

/*! \internal */
QRenderTargetPrivate::QRenderTargetPrivate()
    : QComponentPrivate()
{
}

/*!
    The constructor creates a new QRenderTarget::QRenderTarget instance with
    the specified \a parent.
 */
QRenderTarget::QRenderTarget(QNode *parent)
    : QComponent(*new QRenderTargetPrivate, parent)
{
}

/*! \internal */
QRenderTarget::~QRenderTarget()
{
}

/*! \internal */
QRenderTarget::QRenderTarget(QRenderTargetPrivate &dd, QNode *parent)
    : QComponent(dd, parent)
{
}

/*!
    Adds a chosen output via \a output.
 */
void QRenderTarget::addOutput(QRenderTargetOutput *output)
{
    Q_D(QRenderTarget);
    if (output && !d->m_outputs.contains(output)) {
        d->m_outputs.append(output);

        // Ensures proper bookkeeping
        d->registerDestructionHelper(output, &QRenderTarget::removeOutput, d->m_outputs);

        if (!output->parent())
            output->setParent(this);

        if (d->m_changeArbiter != nullptr) {
            const auto change = QPropertyNodeAddedChangePtr::create(id(), output);
            change->setPropertyName("output");
            d->notifyObservers(change);
        }
    }
}

/*!
    Removes a chosen output via \a output.
 */
void QRenderTarget::removeOutput(QRenderTargetOutput *output)
{
    Q_D(QRenderTarget);

    if (output && d->m_changeArbiter != nullptr) {
        const auto change = QPropertyNodeRemovedChangePtr::create(id(), output);
        change->setPropertyName("output");
        d->notifyObservers(change);
    }
    d->m_outputs.removeOne(output);
    // Remove bookkeeping connection
    d->unregisterDestructionHelper(output);
}

/*!
    \return the chosen outputs.
 */
QVector<QRenderTargetOutput *> QRenderTarget::outputs() const
{
    Q_D(const QRenderTarget);
    return d->m_outputs;
}

Qt3DCore::QNodeCreatedChangeBasePtr QRenderTarget::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QRenderTargetData>::create(this);
    auto &data = creationChange->data;
    data.outputIds = qIdsForNodes(outputs());
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
