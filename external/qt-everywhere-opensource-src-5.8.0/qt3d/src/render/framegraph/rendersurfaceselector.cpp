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

#include "rendersurfaceselector_p.h"
#include <Qt3DRender/qrendersurfaceselector.h>
#include <Qt3DRender/private/qrendersurfaceselector_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

#include <QtGui/qwindow.h>
#include <QtGui/qscreen.h>
#include <QtGui/qoffscreensurface.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace {

QSurface *surfaceFromQObject(QObject *o)
{
    QSurface *surface = nullptr;
    QWindow *window = qobject_cast<QWindow *>(o);
    if (window) {
        surface = static_cast<QSurface *>(window);
    } else {
        QOffscreenSurface *offscreen = qobject_cast<QOffscreenSurface *>(o);
        if (offscreen)
            surface = static_cast<QSurface *>(offscreen);
    }
    return surface;
}

}

namespace Qt3DRender {
namespace Render {

RenderSurfaceSelector::RenderSurfaceSelector()
    : FrameGraphNode(FrameGraphNode::Surface)
    , m_surface(nullptr)
    , m_width(0)
    , m_height(0)
    , m_devicePixelRatio(0.0f)
{
}

void RenderSurfaceSelector::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    FrameGraphNode::initializeFromPeer(change);
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QRenderSurfaceSelectorData>>(change);
    const auto &data = typedChange->data;
    m_surface = surfaceFromQObject(data.surface);
    m_renderTargetSize = data.externalRenderTargetSize;
    m_devicePixelRatio = data.surfacePixelRatio;

    if (m_surface && m_surface->surfaceClass() == QSurface::Window) {
        QWindow *window = static_cast<QWindow *>(m_surface);
        m_width = window->width();
        m_height = window->height();
    }
}

void RenderSurfaceSelector::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    qCDebug(Render::Framegraph) << Q_FUNC_INFO;
    if (e->type() == PropertyUpdated) {
        QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("surface"))
            m_surface = surfaceFromQObject(propertyChange->value().value<QObject *>());
        else if (propertyChange->propertyName() == QByteArrayLiteral("externalRenderTargetSize"))
            setRenderTargetSize(propertyChange->value().toSize());
        else if (propertyChange->propertyName() == QByteArrayLiteral("width"))
            m_width = propertyChange->value().toInt();
        else if (propertyChange->propertyName() == QByteArrayLiteral("height"))
            m_height = propertyChange->value().toInt();
        else if (propertyChange->propertyName() == QByteArrayLiteral("surfacePixelRatio"))
            m_devicePixelRatio = propertyChange->value().toFloat();
        markDirty(AbstractRenderer::AllDirty);
    }
    FrameGraphNode::sceneChangeEvent(e);
}

QSize RenderSurfaceSelector::renderTargetSize() const
{
    if (m_renderTargetSize.isValid())
        return m_renderTargetSize;
    if (m_surface && m_surface->size().isValid())
        return m_surface->size();
    return QSize();
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
