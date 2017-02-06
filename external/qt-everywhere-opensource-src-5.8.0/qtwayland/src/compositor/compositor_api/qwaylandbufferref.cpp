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

#include <QDebug>
#include <QAtomicInt>

#include "qwaylandbufferref.h"
#include "wayland_wrapper/qwlclientbuffer_p.h"

QT_BEGIN_NAMESPACE

class QWaylandBufferRefPrivate
{
public:
    QtWayland::ClientBuffer *buffer;

    bool nullOrDestroyed() {
        return !buffer || buffer->isDestroyed();
    }
};

/*!
 * \class QWaylandBufferRef
 * \inmodule QtWaylandCompositor
 * \since 5.8
 * \brief The QWaylandBufferRef class holds the reference to a surface buffer
 *
 * This class can be used to reference a surface buffer. As long as a reference
 * to the buffer exists, it is owned by the compositor and the client cannot modify it.
 */

/*!
 * Constructs a null buffer ref.
 */
QWaylandBufferRef::QWaylandBufferRef()
                 : d(new QWaylandBufferRefPrivate)
{
    d->buffer = 0;
}

/*!
 * Constructs a reference to \a buffer.
 */
QWaylandBufferRef::QWaylandBufferRef(QtWayland::ClientBuffer *buffer)
                 : d(new QWaylandBufferRefPrivate)
{
    d->buffer = buffer;
    if (buffer)
        buffer->ref();
}

/*!
 * Creates a new reference to the buffer referenced by \a ref.
 */
QWaylandBufferRef::QWaylandBufferRef(const QWaylandBufferRef &ref)
                 : d(new QWaylandBufferRefPrivate)
{
    d->buffer = ref.d->buffer;
    if (d->buffer)
        d->buffer->ref();
}

/*!
 * Dereferences the buffer.
 */
QWaylandBufferRef::~QWaylandBufferRef()
{
    if (d->buffer)
        d->buffer->deref();
    delete d;
}

/*!
 * Assigns \a ref to this buffer and adds a reference to it. The previously referenced buffer is
 * dereferenced.
 */
QWaylandBufferRef &QWaylandBufferRef::operator=(const QWaylandBufferRef &ref)
{
    if (ref.d->buffer)
        ref.d->buffer->ref();

    if (d->buffer)
        d->buffer->deref();

    d->buffer = ref.d->buffer;

    return *this;
}

/*!
 * Returns true if this QWaylandBufferRef references the same buffer as \a ref.
 * Otherwise returns false.
 */
bool QWaylandBufferRef::operator==(const QWaylandBufferRef &ref)
{
    return d->buffer == ref.d->buffer;
}

/*!
 * Returns false if this QWaylandBufferRef references the same buffer as \a ref.
 * Otherwise returns true.
 */
bool QWaylandBufferRef::operator!=(const QWaylandBufferRef &ref)
{
    return d->buffer != ref.d->buffer;
}

/*!
 * Returns true if this QWaylandBufferRef does not reference a buffer.
 * Otherwise returns false.
 *
 * \sa hasBuffer(), hasContent()
 */
bool QWaylandBufferRef::isNull() const
{
    return !d->buffer;
}

/*!
 * Returns true if this QWaylandBufferRef references a buffer. Otherwise returns false.
 *
 * \sa isNull(), hasContent()
 */
bool QWaylandBufferRef::hasBuffer() const
{
    return d->buffer;
}
/*!
 * Returns true if this QWaylandBufferRef references a buffer that has content. Otherwise returns false.
 *
 * \sa isNull(), hasBuffer()
 */
bool QWaylandBufferRef::hasContent() const
{
    return QtWayland::ClientBuffer::hasContent(d->buffer);
}

/*!
 * Returns true if this QWaylandBufferRef references a buffer that
 * has been destroyed. Otherwise returns false.
 */
bool QWaylandBufferRef::isDestroyed() const
{
    return d->buffer && d->buffer->isDestroyed();
}

/*!
 * Returns the Wayland resource for the buffer.
 */
struct ::wl_resource *QWaylandBufferRef::wl_buffer() const
{
    return d->buffer ? d->buffer->waylandBufferHandle() : Q_NULLPTR;
}

/*!
 * \internal
 */
QtWayland::ClientBuffer *QWaylandBufferRef::buffer() const
{
    return d->buffer;
}

/*!
 * Returns the size of the buffer.
 * If the buffer referenced is null, an invalid QSize() is returned.
 */
QSize QWaylandBufferRef::size() const
{
    if (d->nullOrDestroyed())
        return QSize();

    return d->buffer->size();
}

/*!
 * Returns the origin of the buffer.
 * If the buffer referenced is null, QWaylandSurface::OriginBottomLeft
 * is returned.
 */
QWaylandSurface::Origin QWaylandBufferRef::origin() const
{
    if (d->buffer)
        return d->buffer->origin();

    return QWaylandSurface::OriginBottomLeft;
}

QWaylandBufferRef::BufferType QWaylandBufferRef::bufferType() const
{
    if (d->nullOrDestroyed())
        return BufferType_Null;

    if (isSharedMemory())
        return BufferType_SharedMemory;

    return BufferType_Egl;
}

QWaylandBufferRef::BufferFormatEgl QWaylandBufferRef::bufferFormatEgl() const
{
    if (d->nullOrDestroyed())
        return BufferFormatEgl_Null;

    return d->buffer->bufferFormatEgl();
}

/*!
 * Returns true if the buffer is a shared memory buffer. Otherwise returns false.
 */
bool QWaylandBufferRef::isSharedMemory() const
{
    if (d->nullOrDestroyed())
        return false;

    return d->buffer->isSharedMemory();
}

/*!
 * Returns an image with the contents of the buffer.
 */
QImage QWaylandBufferRef::image() const
{
    if (d->nullOrDestroyed())
        return QImage();

    return d->buffer->image();
}

#if QT_CONFIG(opengl)
/*!
 * Returns an OpenGL texture for the buffer. \a plane is the index for multi-plane formats, such as YUV.
 *
 * The returned texture is owned by the buffer. The texture is only valid for as
 * long as the buffer reference exists. The caller of this function must not delete the texture, and must
 * keep a reference to the buffer for as long as the texture is being used.
 *
 * Returns \c nullptr if there is no valid buffer, or if no texture can be created.
 */
QOpenGLTexture *QWaylandBufferRef::toOpenGLTexture(int plane) const
{
    if (d->nullOrDestroyed())
        return nullptr;

    return d->buffer->toOpenGlTexture(plane);
}

/*!
 * Returns the native handle for this buffer, and marks it as locked so it will not be
 * released until unlockNativeBuffer() is called.
 *
 * Returns 0 if there is no native handle for this buffer, or if the lock was unsuccessful.
 */
quintptr QWaylandBufferRef::lockNativeBuffer()
{
    return d->buffer->lockNativeBuffer();
}

/*!
 * Marks the native buffer as no longer in use. \a handle must correspond to the value returned by
 * a previous call to lockNativeBuffer().
 */
void QWaylandBufferRef::unlockNativeBuffer(quintptr handle)
{
    d->buffer->unlockNativeBuffer(handle);
}

#endif

QT_END_NAMESPACE
