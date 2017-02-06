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

#include "platformsurfacefilter_p.h"

#include <Qt3DRender/private/renderer_p.h>

#include <QMetaObject>
#include <QPlatformSurfaceEvent>
#include <QOffscreenSurface>
#include <QtGui/qwindow.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

QSemaphore PlatformSurfaceFilter::m_surfacesSemaphore(1);
QHash<QSurface *, bool> PlatformSurfaceFilter::m_surfacesValidity;

// Surface protection
// The surface is accessible from multiple threads at 3 potential places
// 1) In here (the frontend) when we receive an event
// 2) In the RenderViewJobs
// 3) In the Renderer for the submission
// * We don't need any protection in 2) as we are just copying the pointer
// but not performing any access to the texture as all the information
// we need has been cached in the RenderSurfaceSelector element
// * This leaves us with case 1 and 3. It is important that if the surface
// is about to be destroyed that we let the time to the submission thread
// to complete whatever it is doing with a surface before we have the time
// to process the AboutToBeDestroyed event. For that we have lockSurface, releaseSurface
// on the PlatformSurfaceFilter. But that's not enough, you need to be sure that
// the surface is still valid. When locked, you can use isSurfaceValid to check
// if a surface is still accessible.
// A Surface is valid when it has been created and becomes invalid when AboutToBeDestroyed
// has been called
// SurfaceLocker is a convenience type to perform locking an surface validity check

PlatformSurfaceFilter::PlatformSurfaceFilter(QObject *parent)
    : QObject(parent)
    , m_obj(nullptr)
    , m_surface(nullptr)
{
    qRegisterMetaType<QSurface *>("QSurface*");
}

PlatformSurfaceFilter::~PlatformSurfaceFilter()
{
    if (m_obj)
        m_obj->removeEventFilter(this);
}

bool PlatformSurfaceFilter::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == m_obj && e->type() == QEvent::PlatformSurface) {
        QPlatformSurfaceEvent *ev = static_cast<QPlatformSurfaceEvent *>(e);

        switch (ev->surfaceEventType()) {
        case QPlatformSurfaceEvent::SurfaceCreated:
            // set validy to true
        {
            markSurfaceAsValid();
            break;
        }

        case QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed:
            // set validity to false
        {
            SurfaceLocker lock(m_surface);
            // If we remove it, the call to isSurfaceValid will
            // implicitely return false
            PlatformSurfaceFilter::m_surfacesValidity.remove(m_surface);
            break;
        }

        default:
            qCritical("Unknown surface type");
            Q_UNREACHABLE();
        }
    }

    if (obj == m_obj && e->type() == QEvent::Expose) {
        QExposeEvent *ev = static_cast<QExposeEvent *>(e);
        Q_UNUSED(ev);
        // We could use this to tell to ignore the RenderView
        // at submission time since it's not exposed
    }
    return false;
}

void PlatformSurfaceFilter::lockSurface()
{
    PlatformSurfaceFilter::m_surfacesSemaphore.acquire(1);
}

void PlatformSurfaceFilter::releaseSurface()
{
    PlatformSurfaceFilter::m_surfacesSemaphore.release(1);
}

bool PlatformSurfaceFilter::isSurfaceValid(QSurface *surface)
{
    // Should be called only when the surface is locked
    // with the semaphore
    return m_surfacesValidity.value(surface, false);
}

void PlatformSurfaceFilter::markSurfaceAsValid()
{
    SurfaceLocker lock(m_surface);
    PlatformSurfaceFilter::m_surfacesValidity.insert(m_surface, true);
}

SurfaceLocker::SurfaceLocker(QSurface *surface)
    : m_surface(surface)
{
    PlatformSurfaceFilter::lockSurface();
}

SurfaceLocker::~SurfaceLocker()
{
    PlatformSurfaceFilter::releaseSurface();
}

bool SurfaceLocker::isSurfaceValid() const
{
    return PlatformSurfaceFilter::isSurfaceValid(m_surface);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
