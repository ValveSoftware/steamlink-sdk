/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "qabstractvideobuffer_p.h"

#include <qvariant.h>

#include <QDebug>


QT_BEGIN_NAMESPACE

static void qRegisterAbstractVideoBufferMetaTypes()
{
    qRegisterMetaType<QAbstractVideoBuffer::HandleType>();
    qRegisterMetaType<QAbstractVideoBuffer::MapMode>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterAbstractVideoBufferMetaTypes)

int QAbstractVideoBufferPrivate::map(
            QAbstractVideoBuffer::MapMode mode,
            int *numBytes,
            int bytesPerLine[4],
            uchar *data[4])
{
    data[0] = q_ptr->map(mode, numBytes, bytesPerLine);
    return data[0] ? 1 : 0;
}

/*!
    \class QAbstractVideoBuffer
    \brief The QAbstractVideoBuffer class is an abstraction for video data.
    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_video

    The QVideoFrame class makes use of a QAbstractVideoBuffer internally to reference a buffer of
    video data.  Quite often video data buffers may reside in video memory rather than system
    memory, and this class provides an abstraction of the location.

    In addition, creating a subclass of QAbstractVideoBuffer will allow you to construct video
    frames from preallocated or static buffers, in cases where the QVideoFrame constructors
    taking a QByteArray or a QImage do not suffice.  This may be necessary when implementing
    a new hardware accelerated video system, for example.

    The contents of a buffer can be accessed by mapping the buffer to memory using the map()
    function, which returns a pointer to memory containing the contents of the video buffer.
    The memory returned by map() is released by calling the unmap() function.

    The handle() of a buffer may also be used to manipulate its contents using type specific APIs.
    The type of a buffer's handle is given by the handleType() function.

    \sa QVideoFrame
*/

/*!
    \enum QAbstractVideoBuffer::HandleType

    Identifies the type of a video buffers handle.

    \value NoHandle The buffer has no handle, its data can only be accessed by mapping the buffer.
    \value GLTextureHandle The handle of the buffer is an OpenGL texture ID.
    \value XvShmImageHandle The handle contains pointer to shared memory XVideo image.
    \value CoreImageHandle The handle contains pointer to \macos CIImage.
    \value QPixmapHandle The handle of the buffer is a QPixmap.
    \value EGLImageHandle The handle of the buffer is an EGLImageKHR.
    \value UserHandle Start value for user defined handle types.

    \sa handleType()
*/

/*!
    \enum QAbstractVideoBuffer::MapMode

    Enumerates how a video buffer's data is mapped to system memory.

    \value NotMapped The video buffer is not mapped to memory.
    \value ReadOnly The mapped memory is populated with data from the video buffer when mapped, but
    the content of the mapped memory may be discarded when unmapped.
    \value WriteOnly The mapped memory is uninitialized when mapped, but the possibly modified content
    will be used to populate the video buffer when unmapped.
    \value ReadWrite The mapped memory is populated with data from the video buffer, and the
    video buffer is repopulated with the content of the mapped memory when it is unmapped.

    \sa mapMode(), map()
*/

/*!
    Constructs an abstract video buffer of the given \a type.
*/
QAbstractVideoBuffer::QAbstractVideoBuffer(HandleType type)
    : d_ptr(0)
    , m_type(type)
{
}

/*!
    \internal
*/
QAbstractVideoBuffer::QAbstractVideoBuffer(QAbstractVideoBufferPrivate &dd, HandleType type)
    : d_ptr(&dd)
    , m_type(type)
{
    d_ptr->q_ptr = this;
}

/*!
    Destroys an abstract video buffer.
*/
QAbstractVideoBuffer::~QAbstractVideoBuffer()
{
    delete d_ptr;
}

/*!
    Releases the video buffer.

    QVideoFrame calls QAbstractVideoBuffer::release when the buffer is not used
    any more and can be destroyed or returned to the buffer pool.

    The default implementation deletes the buffer instance.
*/
void QAbstractVideoBuffer::release()
{
    delete this;
}

/*!
    Returns the type of a video buffer's handle.

    \sa handle()
*/
QAbstractVideoBuffer::HandleType QAbstractVideoBuffer::handleType() const
{
    return m_type;
}

/*!
    \fn QAbstractVideoBuffer::mapMode() const

    Returns the mode a video buffer is mapped in.

    \sa map()
*/

/*!
    \fn QAbstractVideoBuffer::map(MapMode mode, int *numBytes, int *bytesPerLine)

    Maps the contents of a video buffer to memory.

    In some cases the video buffer might be stored in video memory or otherwise inaccessible
    memory, so it is necessary to map the buffer before accessing the pixel data.  This may involve
    copying the contents around, so avoid mapping and unmapping unless required.

    The map \a mode indicates whether the contents of the mapped memory should be read from and/or
    written to the buffer.  If the map mode includes the \c QAbstractVideoBuffer::ReadOnly flag the
    mapped memory will be populated with the content of the buffer when initially mapped.  If the map
    mode includes the \c QAbstractVideoBuffer::WriteOnly flag the content of the possibly modified
    mapped memory will be written back to the buffer when unmapped.

    When access to the data is no longer needed be sure to call the unmap() function to release the
    mapped memory and possibly update the buffer contents.

    Returns a pointer to the mapped memory region, or a null pointer if the mapping failed.  The
    size in bytes of the mapped memory region is returned in \a numBytes, and the line stride in \a
    bytesPerLine.

    \note Writing to memory that is mapped as read-only is undefined, and may result in changes
    to shared data or crashes.

    \sa unmap(), mapMode()
*/


/*!
    Independently maps the planes of a video buffer to memory.

    The map \a mode indicates whether the contents of the mapped memory should be read from and/or
    written to the buffer.  If the map mode includes the \c QAbstractVideoBuffer::ReadOnly flag the
    mapped memory will be populated with the content of the buffer when initially mapped.  If the map
    mode includes the \c QAbstractVideoBuffer::WriteOnly flag the content of the possibly modified
    mapped memory will be written back to the buffer when unmapped.

    When access to the data is no longer needed be sure to call the unmap() function to release the
    mapped memory and possibly update the buffer contents.

    Returns the number of planes in the mapped video data.  For each plane the line stride of that
    plane will be returned in \a bytesPerLine, and a pointer to the plane data will be returned in
    \a data.  The accumulative size of the mapped data is returned in \a numBytes.

    Not all buffer implementations will map more than the first plane, if this returns a single
    plane for a planar format the additional planes will have to be calculated from the line stride
    of the first plane and the frame height.  Mapping a buffer with QVideoFrame will do this for
    you.

    To implement this function create a derivative of QAbstractPlanarVideoBuffer and implement
    its map function instance instead.

    \since 5.4
*/
int QAbstractVideoBuffer::mapPlanes(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4])
{
    if (d_ptr) {
        return d_ptr->map(mode, numBytes, bytesPerLine, data);
    } else {
        data[0] = map(mode, numBytes, bytesPerLine);

        return data[0] ? 1 : 0;
    }
}

/*!
    \fn QAbstractVideoBuffer::unmap()

    Releases the memory mapped by the map() function.

    If the \l {QAbstractVideoBuffer::MapMode}{MapMode} included the \c QAbstractVideoBuffer::WriteOnly
    flag this will write the current content of the mapped memory back to the video frame.

    \sa map()
*/

/*!
    Returns a type specific handle to the data buffer.

    The type of the handle is given by handleType() function.

    \sa handleType()
*/
QVariant QAbstractVideoBuffer::handle() const
{
    return QVariant();
}


int QAbstractPlanarVideoBufferPrivate::map(
        QAbstractVideoBuffer::MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4])
{
    return q_func()->map(mode, numBytes, bytesPerLine, data);
}

/*!
    \class QAbstractPlanarVideoBuffer
    \brief The QAbstractPlanarVideoBuffer class is an abstraction for planar video data.
    \inmodule QtMultimedia
    \ingroup QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_video

    QAbstractPlanarVideoBuffer extends QAbstractVideoBuffer to support mapping
    non-continuous planar video data.  Implement this instead of QAbstractVideoBuffer when the
    abstracted video data stores planes in separate buffers or includes padding between planes
    which would interfere with calculating offsets from the bytes per line and frame height.

    \sa QAbstractVideoBuffer::mapPlanes()
    \since 5.4
*/

/*!
    Constructs an abstract planar video buffer of the given \a type.
*/
QAbstractPlanarVideoBuffer::QAbstractPlanarVideoBuffer(HandleType type)
    : QAbstractVideoBuffer(*new QAbstractPlanarVideoBufferPrivate, type)
{
}

/*!
    \internal
*/
QAbstractPlanarVideoBuffer::QAbstractPlanarVideoBuffer(
        QAbstractPlanarVideoBufferPrivate &dd, HandleType type)
    : QAbstractVideoBuffer(dd, type)
{
}
/*!
    Destroys an abstract planar video buffer.
*/
QAbstractPlanarVideoBuffer::~QAbstractPlanarVideoBuffer()
{
}

/*!
    \internal
*/
uchar *QAbstractPlanarVideoBuffer::map(MapMode mode, int *numBytes, int *bytesPerLine)
{
    uchar *data[4];
    int strides[4];
    if (map(mode, numBytes, strides, data) > 0) {
        if (bytesPerLine)
            *bytesPerLine = strides[0];
        return data[0];
    } else {
        return 0;
    }
}

/*!
    \fn int QAbstractPlanarVideoBuffer::map(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4])

    Maps the contents of a video buffer to memory.

    The map \a mode indicates whether the contents of the mapped memory should be read from and/or
    written to the buffer.  If the map mode includes the \c QAbstractVideoBuffer::ReadOnly flag the
    mapped memory will be populated with the content of the buffer when initially mapped.  If the map
    mode includes the \c QAbstractVideoBuffer::WriteOnly flag the content of the possibly modified
    mapped memory will be written back to the buffer when unmapped.

    When access to the data is no longer needed be sure to call the unmap() function to release the
    mapped memory and possibly update the buffer contents.

    Returns the number of planes in the mapped video data.  For each plane the line stride of that
    plane will be returned in \a bytesPerLine, and a pointer to the plane data will be returned in
    \a data.  The accumulative size of the mapped data is returned in \a numBytes.

    \sa QAbstractVideoBuffer::map(), QAbstractVideoBuffer::unmap(), QAbstractVideoBuffer::mapMode()
*/

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, QAbstractVideoBuffer::HandleType type)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (type) {
    case QAbstractVideoBuffer::NoHandle:
        return dbg << "NoHandle";
    case QAbstractVideoBuffer::GLTextureHandle:
        return dbg << "GLTextureHandle";
    case QAbstractVideoBuffer::XvShmImageHandle:
        return dbg << "XvShmImageHandle";
    case QAbstractVideoBuffer::CoreImageHandle:
        return dbg << "CoreImageHandle";
    case QAbstractVideoBuffer::QPixmapHandle:
        return dbg << "QPixmapHandle";
    default:
        return dbg << "UserHandle(" << int(type) << ')';
    }
}

QDebug operator<<(QDebug dbg, QAbstractVideoBuffer::MapMode mode)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (mode) {
    case QAbstractVideoBuffer::ReadOnly:
        return dbg << "ReadOnly";
    case QAbstractVideoBuffer::ReadWrite:
        return dbg << "ReadWrite";
    case QAbstractVideoBuffer::WriteOnly:
        return dbg << "WriteOnly";
    default:
        return dbg << "NotMapped";
    }
}
#endif

QT_END_NAMESPACE
