/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <giulio.camuffo@jollamobile.com>
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

#include <QSGTexture>
#include <QOpenGLTexture>
#include <QQuickWindow>
#include <QDebug>

#include "qwaylandquicksurface.h"
#include "qwaylandquickcompositor.h"
#include "qwaylandquickitem.h"
#include <QtWaylandCompositor/qwaylandbufferref.h>
#include <QtWaylandCompositor/QWaylandView>
#include <QtWaylandCompositor/private/qwaylandsurface_p.h>

#include <QtWaylandCompositor/private/qwayland-server-surface-extension.h>
#include <QtWaylandCompositor/private/qwlextendedsurface_p.h>

QT_BEGIN_NAMESPACE

class QWaylandQuickSurfacePrivate : public QWaylandSurfacePrivate
{
    Q_DECLARE_PUBLIC(QWaylandQuickSurface)
public:
    QWaylandQuickSurfacePrivate()
        : QWaylandSurfacePrivate()
        , useTextureAlpha(true)
        , clientRenderingEnabled(true)
    {
    }

    ~QWaylandQuickSurfacePrivate()
    {
    }

    bool useTextureAlpha;
    bool clientRenderingEnabled;
};

QWaylandQuickSurface::QWaylandQuickSurface()
    : QWaylandSurface(* new QWaylandQuickSurfacePrivate())
{

}
QWaylandQuickSurface::QWaylandQuickSurface(QWaylandCompositor *compositor, QWaylandClient *client, quint32 id, int version)
                    : QWaylandSurface(* new QWaylandQuickSurfacePrivate())
{
    initialize(compositor, client, id, version);
}

QWaylandQuickSurface::~QWaylandQuickSurface()
{

}

/*!
 * \qmlproperty bool QtWaylandCompositor::WaylandSurface::useTextureAlpha
 *
 * This property specifies whether the surface should use texture alpha.
 */
bool QWaylandQuickSurface::useTextureAlpha() const
{
    Q_D(const QWaylandQuickSurface);
    return d->useTextureAlpha;
}

void QWaylandQuickSurface::setUseTextureAlpha(bool useTextureAlpha)
{
    Q_D(QWaylandQuickSurface);
    if (d->useTextureAlpha != useTextureAlpha) {
        d->useTextureAlpha = useTextureAlpha;
        emit useTextureAlphaChanged();
        emit configure(d->bufferRef.hasBuffer());
    }
}

/*!
 * \qmlproperty bool QtWaylandCompositor::WaylandSurface::clientRenderingEnabled
 *
 * This property specifies whether client rendering is enabled for the surface.
 */
bool QWaylandQuickSurface::clientRenderingEnabled() const
{
    Q_D(const QWaylandQuickSurface);
    return d->clientRenderingEnabled;
}

void QWaylandQuickSurface::setClientRenderingEnabled(bool enabled)
{
    Q_D(QWaylandQuickSurface);
    if (d->clientRenderingEnabled != enabled) {
        d->clientRenderingEnabled = enabled;

        if (QtWayland::ExtendedSurface *extSurface = QtWayland::ExtendedSurface::findIn(this))
            extSurface->setVisibility(enabled ? QWindow::AutomaticVisibility : QWindow::Hidden);

        emit clientRenderingEnabledChanged();
    }
}

QT_END_NAMESPACE
