/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwaylandsurfacegrabber.h"

#include <QtCore/private/qobject_p.h>
#include <QtWaylandCompositor/qwaylandsurface.h>
#include <QtWaylandCompositor/qwaylandcompositor.h>
#include <QtWaylandCompositor/private/qwaylandsurface_p.h>

QT_BEGIN_NAMESPACE

/*!
    \class QWaylandSurfaceGrabber
    \inmodule QtWaylandCompositor
    \since 5.8
    \brief The QWaylandSurfaceGrabber class allows to read the content of a QWaylandSurface

    Sometimes it is needed to get the contents of a surface, for example to provide a screenshot
    to the user. The QWaylandSurfaceGrabber class provides a simple method to do so, without
    having to care what type of buffer backs the surface, be it shared memory, OpenGL or something
    else.
*/

/*!
    \enum QWaylandSurfaceGrabber::Error

    The Error enum describes the reason for a grab failure.

    \value InvalidSurface The surface is null or otherwise not valid.
    \value NoBufferAttached The client has not attached a buffer on the surface yet.
    \value UnknownBufferType The buffer attached on the surface is of an unknown type.
    \value RendererNotReady The compositor renderer is not ready to grab the surface content.
 */

class QWaylandSurfaceGrabberPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QWaylandSurfaceGrabber)

    QWaylandSurface *surface;
};

/*!
 * Create a QWaylandSurfaceGrabber object with the given \a surface and \a parent
 */
QWaylandSurfaceGrabber::QWaylandSurfaceGrabber(QWaylandSurface *surface, QObject *parent)
                      : QObject(*(new QWaylandSurfaceGrabberPrivate), parent)
{
    Q_D(QWaylandSurfaceGrabber);
    d->surface = surface;
}

/*!
 * Returns the surface set on this object
 */
QWaylandSurface *QWaylandSurfaceGrabber::surface() const
{
    Q_D(const QWaylandSurfaceGrabber);
    return d->surface;
}

/*!
 * Grab the content of the surface set on this object.
 * It may not be possible to do that immediately so the success and failed signals
 * should be used to be notified of when the grab is completed.
 */
void QWaylandSurfaceGrabber::grab()
{
    Q_D(QWaylandSurfaceGrabber);
    if (!d->surface) {
        emit failed(InvalidSurface);
        return;
    }

    QWaylandSurfacePrivate *surf = QWaylandSurfacePrivate::get(d->surface);
    QWaylandBufferRef buf = surf->bufferRef;
    if (!buf.hasBuffer()) {
        emit failed(NoBufferAttached);
        return;
    }

    d->surface->compositor()->grabSurface(this, buf);
}

QT_END_NAMESPACE
