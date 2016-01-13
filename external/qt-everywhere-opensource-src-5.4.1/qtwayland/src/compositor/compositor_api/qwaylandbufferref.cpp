/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <giulio.camuffo@jollamobile.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QDebug>
#include <QAtomicInt>

#include "qwaylandbufferref.h"
#include "wayland_wrapper/qwlsurfacebuffer_p.h"

QT_BEGIN_NAMESPACE

class QWaylandBufferRefPrivate
{
public:
    QtWayland::SurfaceBuffer *buffer;
};

QWaylandBufferRef::QWaylandBufferRef()
                 : d(new QWaylandBufferRefPrivate)
{
    d->buffer = 0;
}

QWaylandBufferRef::QWaylandBufferRef(QtWayland::SurfaceBuffer *buffer)
                 : d(new QWaylandBufferRefPrivate)
{
    d->buffer = buffer;
    if (buffer)
        buffer->ref();
}

QWaylandBufferRef::QWaylandBufferRef(const QWaylandBufferRef &ref)
                 : d(new QWaylandBufferRefPrivate)
{
    d->buffer = 0;
    *this = ref;
}

QWaylandBufferRef::~QWaylandBufferRef()
{
    if (d->buffer)
        d->buffer->deref();
    delete d;
}

QWaylandBufferRef &QWaylandBufferRef::operator=(const QWaylandBufferRef &ref)
{
    if (d->buffer)
        d->buffer->deref();

    d->buffer = ref.d->buffer;
    if (d->buffer)
        d->buffer->ref();

    return *this;
}

QWaylandBufferRef::operator bool() const
{
    return d->buffer && d->buffer->waylandBufferHandle();
}

bool QWaylandBufferRef::isShm() const
{
    return d->buffer->isShmBuffer();
}

QImage QWaylandBufferRef::image() const
{
    if (d->buffer->isShmBuffer())
        return d->buffer->image();
    return QImage();
}

#ifdef QT_COMPOSITOR_WAYLAND_GL

GLuint QWaylandBufferRef::createTexture()
{
    if (!d->buffer->isShmBuffer() && !d->buffer->textureCreated()) {
        d->buffer->createTexture();
    }
    return d->buffer->texture();
}

void QWaylandBufferRef::destroyTexture()
{
    if (!d->buffer->isShmBuffer() && d->buffer->textureCreated()) {
        d->buffer->destroyTexture();
    }
}
#endif

QT_END_NAMESPACE
