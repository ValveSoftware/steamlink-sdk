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

#include "qvideoframe.h"

#include "qvideoframe_p.h"
#include "qimagevideobuffer_p.h"
#include "qmemoryvideobuffer_p.h"
#include "qvideoframeconversionhelper_p.h"

#include <qimage.h>
#include <qpair.h>
#include <qsize.h>
#include <qvariant.h>
#include <qvector.h>
#include <qmutex.h>

#include <QDebug>

QT_BEGIN_NAMESPACE

static void qRegisterVideoFrameMetaTypes()
{
    qRegisterMetaType<QVideoFrame>();
    qRegisterMetaType<QVideoFrame::FieldType>();
    qRegisterMetaType<QVideoFrame::PixelFormat>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterVideoFrameMetaTypes)


class QVideoFramePrivate : public QSharedData
{
public:
    QVideoFramePrivate()
        : startTime(-1)
        , endTime(-1)
        , mappedBytes(0)
        , planeCount(0)
        , pixelFormat(QVideoFrame::Format_Invalid)
        , fieldType(QVideoFrame::ProgressiveFrame)
        , buffer(0)
        , mappedCount(0)
    {
        memset(data, 0, sizeof(data));
        memset(bytesPerLine, 0, sizeof(bytesPerLine));
    }

    QVideoFramePrivate(const QSize &size, QVideoFrame::PixelFormat format)
        : size(size)
        , startTime(-1)
        , endTime(-1)
        , mappedBytes(0)
        , planeCount(0)
        , pixelFormat(format)
        , fieldType(QVideoFrame::ProgressiveFrame)
        , buffer(0)
        , mappedCount(0)
    {
        memset(data, 0, sizeof(data));
        memset(bytesPerLine, 0, sizeof(bytesPerLine));
    }

    ~QVideoFramePrivate()
    {
        if (buffer)
            buffer->release();
    }

    QSize size;
    qint64 startTime;
    qint64 endTime;
    uchar *data[4];
    int bytesPerLine[4];
    int mappedBytes;
    int planeCount;
    QVideoFrame::PixelFormat pixelFormat;
    QVideoFrame::FieldType fieldType;
    QAbstractVideoBuffer *buffer;
    int mappedCount;
    QMutex mapMutex;
    QVariantMap metadata;

private:
    Q_DISABLE_COPY(QVideoFramePrivate)
};

/*!
    \class QVideoFrame
    \brief The QVideoFrame class represents a frame of video data.
    \inmodule QtMultimedia

    \ingroup multimedia
    \ingroup multimedia_video

    A QVideoFrame encapsulates the pixel data of a video frame, and information about the frame.

    Video frames can come from several places - decoded \l {QMediaPlayer}{media}, a
    \l {QCamera}{camera}, or generated programmatically.  The way pixels are described in these
    frames can vary greatly, and some pixel formats offer greater compression opportunities at
    the expense of ease of use.

    The pixel contents of a video frame can be mapped to memory using the map() function.  While
    mapped, the video data can accessed using the bits() function, which returns a pointer to a
    buffer.  The total size of this buffer is given by the mappedBytes() function, and the size of
    each line is given by bytesPerLine().  The return value of the handle() function may also be
    used to access frame data using the internal buffer's native APIs (for example - an OpenGL
    texture handle).

    A video frame can also have timestamp information associated with it.  These timestamps can be
    used by an implementation of \l QAbstractVideoSurface to determine when to start and stop
    displaying the frame, but not all surfaces might respect this setting.

    The video pixel data in a QVideoFrame is encapsulated in a QAbstractVideoBuffer.  A QVideoFrame
    may be constructed from any buffer type by subclassing the QAbstractVideoBuffer class.

    \note Since video frames can be expensive to copy, QVideoFrame is explicitly shared, so any
    change made to a video frame will also apply to any copies.
*/

/*!
    \enum QVideoFrame::PixelFormat

    Enumerates video data types.

    \value Format_Invalid
    The frame is invalid.

    \value Format_ARGB32
    The frame is stored using a 32-bit ARGB format (0xAARRGGBB).  This is equivalent to
    QImage::Format_ARGB32.

    \value Format_ARGB32_Premultiplied
    The frame stored using a premultiplied 32-bit ARGB format (0xAARRGGBB).  This is equivalent
    to QImage::Format_ARGB32_Premultiplied.

    \value Format_RGB32
    The frame stored using a 32-bit RGB format (0xffRRGGBB).  This is equivalent to
    QImage::Format_RGB32

    \value Format_RGB24
    The frame is stored using a 24-bit RGB format (8-8-8).  This is equivalent to
    QImage::Format_RGB888

    \value Format_RGB565
    The frame is stored using a 16-bit RGB format (5-6-5).  This is equivalent to
    QImage::Format_RGB16.

    \value Format_RGB555
    The frame is stored using a 16-bit RGB format (5-5-5).  This is equivalent to
    QImage::Format_RGB555.

    \value Format_ARGB8565_Premultiplied
    The frame is stored using a 24-bit premultiplied ARGB format (8-5-6-5).

    \value Format_BGRA32
    The frame is stored using a 32-bit BGRA format (0xBBGGRRAA).

    \value Format_BGRA32_Premultiplied
    The frame is stored using a premultiplied 32bit BGRA format.

    \value Format_BGR32
    The frame is stored using a 32-bit BGR format (0xBBGGRRff).

    \value Format_BGR24
    The frame is stored using a 24-bit BGR format (0xBBGGRR).

    \value Format_BGR565
    The frame is stored using a 16-bit BGR format (5-6-5).

    \value Format_BGR555
    The frame is stored using a 16-bit BGR format (5-5-5).

    \value Format_BGRA5658_Premultiplied
    The frame is stored using a 24-bit premultiplied BGRA format (5-6-5-8).

    \value Format_AYUV444
    The frame is stored using a packed 32-bit AYUV format (0xAAYYUUVV).

    \value Format_AYUV444_Premultiplied
    The frame is stored using a packed premultiplied 32-bit AYUV format (0xAAYYUUVV).

    \value Format_YUV444
    The frame is stored using a 24-bit packed YUV format (8-8-8).

    \value Format_YUV420P
    The frame is stored using an 8-bit per component planar YUV format with the U and V planes
    horizontally and vertically sub-sampled, i.e. the height and width of the U and V planes are
    half that of the Y plane.

    \value Format_YV12
    The frame is stored using an 8-bit per component planar YVU format with the V and U planes
    horizontally and vertically sub-sampled, i.e. the height and width of the V and U planes are
    half that of the Y plane.

    \value Format_UYVY
    The frame is stored using an 8-bit per component packed YUV format with the U and V planes
    horizontally sub-sampled (U-Y-V-Y), i.e. two horizontally adjacent pixels are stored as a 32-bit
    macropixel which has a Y value for each pixel and common U and V values.

    \value Format_YUYV
    The frame is stored using an 8-bit per component packed YUV format with the U and V planes
    horizontally sub-sampled (Y-U-Y-V), i.e. two horizontally adjacent pixels are stored as a 32-bit
    macropixel which has a Y value for each pixel and common U and V values.

    \value Format_NV12
    The frame is stored using an 8-bit per component semi-planar YUV format with a Y plane (Y)
    followed by a horizontally and vertically sub-sampled, packed UV plane (U-V).

    \value Format_NV21
    The frame is stored using an 8-bit per component semi-planar YUV format with a Y plane (Y)
    followed by a horizontally and vertically sub-sampled, packed VU plane (V-U).

    \value Format_IMC1
    The frame is stored using an 8-bit per component planar YUV format with the U and V planes
    horizontally and vertically sub-sampled.  This is similar to the Format_YUV420P type, except
    that the bytes per line of the U and V planes are padded out to the same stride as the Y plane.

    \value Format_IMC2
    The frame is stored using an 8-bit per component planar YUV format with the U and V planes
    horizontally and vertically sub-sampled.  This is similar to the Format_YUV420P type, except
    that the lines of the U and V planes are interleaved, i.e. each line of U data is followed by a
    line of V data creating a single line of the same stride as the Y data.

    \value Format_IMC3
    The frame is stored using an 8-bit per component planar YVU format with the V and U planes
    horizontally and vertically sub-sampled.  This is similar to the Format_YV12 type, except that
    the bytes per line of the V and U planes are padded out to the same stride as the Y plane.

    \value Format_IMC4
    The frame is stored using an 8-bit per component planar YVU format with the V and U planes
    horizontally and vertically sub-sampled.  This is similar to the Format_YV12 type, except that
    the lines of the V and U planes are interleaved, i.e. each line of V data is followed by a line
    of U data creating a single line of the same stride as the Y data.

    \value Format_Y8
    The frame is stored using an 8-bit greyscale format.

    \value Format_Y16
    The frame is stored using a 16-bit linear greyscale format.  Little endian.

    \value Format_Jpeg
    The frame is stored in compressed Jpeg format.

    \value Format_CameraRaw
    The frame is stored using a device specific camera raw format.

    \value Format_AdobeDng
    The frame is stored using raw Adobe Digital Negative (DNG) format.

    \value Format_User
    Start value for user defined pixel formats.
*/

/*!
    \enum QVideoFrame::FieldType

    Specifies the field an interlaced video frame belongs to.

    \value ProgressiveFrame The frame is not interlaced.
    \value TopField The frame contains a top field.
    \value BottomField The frame contains a bottom field.
    \value InterlacedFrame The frame contains a merged top and bottom field.
*/

/*!
    Constructs a null video frame.
*/
QVideoFrame::QVideoFrame()
    : d(new QVideoFramePrivate)
{
}

/*!
    Constructs a video frame from a \a buffer with the given pixel \a format and \a size in pixels.

    \note This doesn't increment the reference count of the video buffer.
*/
QVideoFrame::QVideoFrame(
        QAbstractVideoBuffer *buffer, const QSize &size, PixelFormat format)
    : d(new QVideoFramePrivate(size, format))
{
    d->buffer = buffer;
}

/*!
    Constructs a video frame of the given pixel \a format and \a size in pixels.

    The \a bytesPerLine (stride) is the length of each scan line in bytes, and \a bytes is the total
    number of bytes that must be allocated for the frame.
*/
QVideoFrame::QVideoFrame(int bytes, const QSize &size, int bytesPerLine, PixelFormat format)
    : d(new QVideoFramePrivate(size, format))
{
    if (bytes > 0) {
        QByteArray data;
        data.resize(bytes);

        // Check the memory was successfully allocated.
        if (!data.isEmpty())
            d->buffer = new QMemoryVideoBuffer(data, bytesPerLine);
    }
}

/*!
    Constructs a video frame from an \a image.

    \note This will construct an invalid video frame if there is no frame type equivalent to the
    image format.

    \sa pixelFormatFromImageFormat()
*/
QVideoFrame::QVideoFrame(const QImage &image)
    : d(new QVideoFramePrivate(
            image.size(), pixelFormatFromImageFormat(image.format())))
{
    if (d->pixelFormat != Format_Invalid)
        d->buffer = new QImageVideoBuffer(image);
}

/*!
    Constructs a shallow copy of \a other.  Since QVideoFrame is
    explicitly shared, these two instances will reflect the same frame.

*/
QVideoFrame::QVideoFrame(const QVideoFrame &other)
    : d(other.d)
{
}

/*!
    Assigns the contents of \a other to this video frame.  Since QVideoFrame is
    explicitly shared, these two instances will reflect the same frame.

*/
QVideoFrame &QVideoFrame::operator =(const QVideoFrame &other)
{
    d = other.d;

    return *this;
}

/*!
  \return \c true if this QVideoFrame and \a other reflect the same frame.
 */
bool QVideoFrame::operator==(const QVideoFrame &other) const
{
    // Due to explicit sharing we just compare the QSharedData which in turn compares the pointers.
    return d == other.d;
}

/*!
  \return \c true if this QVideoFrame and \a other do not reflect the same frame.
 */
bool QVideoFrame::operator!=(const QVideoFrame &other) const
{
    return d != other.d;
}

/*!
    Destroys a video frame.
*/
QVideoFrame::~QVideoFrame()
{
}

/*!
    Identifies whether a video frame is valid.

    An invalid frame has no video buffer associated with it.

    Returns true if the frame is valid, and false if it is not.
*/
bool QVideoFrame::isValid() const
{
    return d->buffer != 0;
}

/*!
    Returns the color format of a video frame.
*/
QVideoFrame::PixelFormat QVideoFrame::pixelFormat() const
{
    return d->pixelFormat;
}

/*!
    Returns the type of a video frame's handle.

*/
QAbstractVideoBuffer::HandleType QVideoFrame::handleType() const
{
    return d->buffer ? d->buffer->handleType() : QAbstractVideoBuffer::NoHandle;
}

/*!
    Returns the dimensions of a video frame.
*/
QSize QVideoFrame::size() const
{
    return d->size;
}

/*!
    Returns the width of a video frame.
*/
int QVideoFrame::width() const
{
    return d->size.width();
}

/*!
    Returns the height of a video frame.
*/
int QVideoFrame::height() const
{
    return d->size.height();
}

/*!
    Returns the field an interlaced video frame belongs to.

    If the video is not interlaced this will return WholeFrame.
*/
QVideoFrame::FieldType QVideoFrame::fieldType() const
{
    return d->fieldType;
}

/*!
    Sets the \a field an interlaced video frame belongs to.
*/
void QVideoFrame::setFieldType(QVideoFrame::FieldType field)
{
    d->fieldType = field;
}

/*!
    Identifies if a video frame's contents are currently mapped to system memory.

    This is a convenience function which checks that the \l {QAbstractVideoBuffer::MapMode}{MapMode}
    of the frame is not equal to QAbstractVideoBuffer::NotMapped.

    Returns true if the contents of the video frame are mapped to system memory, and false
    otherwise.

    \sa mapMode(), QAbstractVideoBuffer::MapMode
*/

bool QVideoFrame::isMapped() const
{
    return d->buffer != 0 && d->buffer->mapMode() != QAbstractVideoBuffer::NotMapped;
}

/*!
    Identifies if the mapped contents of a video frame will be persisted when the frame is unmapped.

    This is a convenience function which checks if the \l {QAbstractVideoBuffer::MapMode}{MapMode}
    contains the QAbstractVideoBuffer::WriteOnly flag.

    Returns true if the video frame will be updated when unmapped, and false otherwise.

    \note The result of altering the data of a frame that is mapped in read-only mode is undefined.
    Depending on the buffer implementation the changes may be persisted, or worse alter a shared
    buffer.

    \sa mapMode(), QAbstractVideoBuffer::MapMode
*/
bool QVideoFrame::isWritable() const
{
    return d->buffer != 0 && (d->buffer->mapMode() & QAbstractVideoBuffer::WriteOnly);
}

/*!
    Identifies if the mapped contents of a video frame were read from the frame when it was mapped.

    This is a convenience function which checks if the \l {QAbstractVideoBuffer::MapMode}{MapMode}
    contains the QAbstractVideoBuffer::WriteOnly flag.

    Returns true if the contents of the mapped memory were read from the video frame, and false
    otherwise.

    \sa mapMode(), QAbstractVideoBuffer::MapMode
*/
bool QVideoFrame::isReadable() const
{
    return d->buffer != 0 && (d->buffer->mapMode() & QAbstractVideoBuffer::ReadOnly);
}

/*!
    Returns the mode a video frame was mapped to system memory in.

    \sa map(), QAbstractVideoBuffer::MapMode
*/
QAbstractVideoBuffer::MapMode QVideoFrame::mapMode() const
{
    return d->buffer != 0 ? d->buffer->mapMode() : QAbstractVideoBuffer::NotMapped;
}

/*!
    Maps the contents of a video frame to system (CPU addressable) memory.

    In some cases the video frame data might be stored in video memory or otherwise inaccessible
    memory, so it is necessary to map a frame before accessing the pixel data.  This may involve
    copying the contents around, so avoid mapping and unmapping unless required.

    The map \a mode indicates whether the contents of the mapped memory should be read from and/or
    written to the frame.  If the map mode includes the \c QAbstractVideoBuffer::ReadOnly flag the
    mapped memory will be populated with the content of the video frame when initially mapped.  If the map
    mode includes the \c QAbstractVideoBuffer::WriteOnly flag the content of the possibly modified
    mapped memory will be written back to the frame when unmapped.

    While mapped the contents of a video frame can be accessed directly through the pointer returned
    by the bits() function.

    When access to the data is no longer needed, be sure to call the unmap() function to release the
    mapped memory and possibly update the video frame contents.

    If the video frame has been mapped in read only mode, it is permissible to map it
    multiple times in read only mode (and unmap it a corresponding number of times). In all
    other cases it is necessary to unmap the frame first before mapping a second time.

    \note Writing to memory that is mapped as read-only is undefined, and may result in changes
    to shared data or crashes.

    Returns true if the frame was mapped to memory in the given \a mode and false otherwise.

    \sa unmap(), mapMode(), bits()
*/
bool QVideoFrame::map(QAbstractVideoBuffer::MapMode mode)
{
    QMutexLocker lock(&d->mapMutex);

    if (!d->buffer)
        return false;

    if (mode == QAbstractVideoBuffer::NotMapped)
        return false;

    if (d->mappedCount > 0) {
        //it's allowed to map the video frame multiple times in read only mode
        if (d->buffer->mapMode() == QAbstractVideoBuffer::ReadOnly
                && mode == QAbstractVideoBuffer::ReadOnly) {
            d->mappedCount++;
            return true;
        } else {
            return false;
        }
    }

    Q_ASSERT(d->data[0] == 0);
    Q_ASSERT(d->bytesPerLine[0] == 0);
    Q_ASSERT(d->planeCount == 0);
    Q_ASSERT(d->mappedBytes == 0);

    d->planeCount = d->buffer->mapPlanes(mode, &d->mappedBytes, d->bytesPerLine, d->data);
    if (d->planeCount == 0)
        return false;

    if (d->planeCount > 1) {
        // If the plane count is derive the additional planes for planar formats.
    } else switch (d->pixelFormat) {
    case Format_Invalid:
    case Format_ARGB32:
    case Format_ARGB32_Premultiplied:
    case Format_RGB32:
    case Format_RGB24:
    case Format_RGB565:
    case Format_RGB555:
    case Format_ARGB8565_Premultiplied:
    case Format_BGRA32:
    case Format_BGRA32_Premultiplied:
    case Format_BGR32:
    case Format_BGR24:
    case Format_BGR565:
    case Format_BGR555:
    case Format_BGRA5658_Premultiplied:
    case Format_AYUV444:
    case Format_AYUV444_Premultiplied:
    case Format_YUV444:
    case Format_UYVY:
    case Format_YUYV:
    case Format_Y8:
    case Format_Y16:
    case Format_Jpeg:
    case Format_CameraRaw:
    case Format_AdobeDng:
    case Format_User:
        // Single plane or opaque format.
        break;
    case Format_YUV420P:
    case Format_YV12: {
        // The UV stride is usually half the Y stride and is 32-bit aligned.
        // However it's not always the case, at least on Windows where the
        // UV planes are sometimes not aligned.
        // We calculate the stride using the UV byte count to always
        // have a correct stride.
        const int height = d->size.height();
        const int yStride = d->bytesPerLine[0];
        const int uvStride = (d->mappedBytes - (yStride * height)) / height;

        // Three planes, the second and third vertically and horizontally subsampled.
        d->planeCount = 3;
        d->bytesPerLine[2] = d->bytesPerLine[1] = uvStride;
        d->data[1] = d->data[0] + (yStride * height);
        d->data[2] = d->data[1] + (uvStride * height / 2);
        break;
    }
    case Format_NV12:
    case Format_NV21:
    case Format_IMC2:
    case Format_IMC4: {
        // Semi planar, Full resolution Y plane with interleaved subsampled U and V planes.
        d->planeCount = 2;
        d->bytesPerLine[1] = d->bytesPerLine[0];
        d->data[1] = d->data[0] + (d->bytesPerLine[0] * d->size.height());
        break;
    }
    case Format_IMC1:
    case Format_IMC3: {
        // Three planes, the second and third vertically and horizontally subsumpled,
        // but with lines padded to the width of the first plane.
        d->planeCount = 3;
        d->bytesPerLine[2] = d->bytesPerLine[1] = d->bytesPerLine[0];
        d->data[1] = d->data[0] + (d->bytesPerLine[0] * d->size.height());
        d->data[2] = d->data[1] + (d->bytesPerLine[1] * d->size.height() / 2);
        break;
    }
    default:
        break;
    }

    d->mappedCount++;
    return true;
}

/*!
    Releases the memory mapped by the map() function.

    If the \l {QAbstractVideoBuffer::MapMode}{MapMode} included the QAbstractVideoBuffer::WriteOnly
    flag this will persist the current content of the mapped memory to the video frame.

    unmap() should not be called if map() function failed.

    \sa map()
*/
void QVideoFrame::unmap()
{
    QMutexLocker lock(&d->mapMutex);

    if (!d->buffer)
        return;

    if (d->mappedCount == 0) {
        qWarning() << "QVideoFrame::unmap() was called more times then QVideoFrame::map()";
        return;
    }

    d->mappedCount--;

    if (d->mappedCount == 0) {
        d->mappedBytes = 0;
        d->planeCount = 0;
        memset(d->bytesPerLine, 0, sizeof(d->bytesPerLine));
        memset(d->data, 0, sizeof(d->data));

        d->buffer->unmap();
    }
}

/*!
    Returns the number of bytes in a scan line.

    \note For planar formats this is the bytes per line of the first plane only.  The bytes per line of subsequent
    planes should be calculated as per the frame \l{QVideoFrame::PixelFormat}{pixel format}.

    This value is only valid while the frame data is \l {map()}{mapped}.

    \sa bits(), map(), mappedBytes()
*/
int QVideoFrame::bytesPerLine() const
{
    return d->bytesPerLine[0];
}

/*!
    Returns the number of bytes in a scan line of a \a plane.

    This value is only valid while the frame data is \l {map()}{mapped}.

    \sa bits(), map(), mappedBytes(), planeCount()
    \since 5.4
*/

int QVideoFrame::bytesPerLine(int plane) const
{
    return plane >= 0 && plane < d->planeCount ? d->bytesPerLine[plane] : 0;
}

/*!
    Returns a pointer to the start of the frame data buffer.

    This value is only valid while the frame data is \l {map()}{mapped}.

    Changes made to data accessed via this pointer (when mapped with write access)
    are only guaranteed to have been persisted when unmap() is called and when the
    buffer has been mapped for writing.

    \sa map(), mappedBytes(), bytesPerLine()
*/
uchar *QVideoFrame::bits()
{
    return d->data[0];
}

/*!
    Returns a pointer to the start of the frame data buffer for a \a plane.

    This value is only valid while the frame data is \l {map()}{mapped}.

    Changes made to data accessed via this pointer (when mapped with write access)
    are only guaranteed to have been persisted when unmap() is called and when the
    buffer has been mapped for writing.

    \sa map(), mappedBytes(), bytesPerLine(), planeCount()
    \since 5.4
*/
uchar *QVideoFrame::bits(int plane)
{
    return plane >= 0 && plane < d->planeCount ? d->data[plane] : 0;
}

/*!
    Returns a pointer to the start of the frame data buffer.

    This value is only valid while the frame data is \l {map()}{mapped}.

    If the buffer was not mapped with read access, the contents of this
    buffer will initially be uninitialized.

    \sa map(), mappedBytes(), bytesPerLine()
*/
const uchar *QVideoFrame::bits() const
{
    return d->data[0];
}

/*!
    Returns a pointer to the start of the frame data buffer for a \a plane.

    This value is only valid while the frame data is \l {map()}{mapped}.

    If the buffer was not mapped with read access, the contents of this
    buffer will initially be uninitialized.

    \sa map(), mappedBytes(), bytesPerLine(), planeCount()
    \since 5.4
*/
const uchar *QVideoFrame::bits(int plane) const
{
    return plane >= 0 && plane < d->planeCount ?  d->data[plane] : 0;
}

/*!
    Returns the number of bytes occupied by the mapped frame data.

    This value is only valid while the frame data is \l {map()}{mapped}.

    \sa map()
*/
int QVideoFrame::mappedBytes() const
{
    return d->mappedBytes;
}

/*!
    Returns the number of planes in the video frame.

    This value is only valid while the frame data is \l {map()}{mapped}.

    \sa map()
    \since 5.4
*/

int QVideoFrame::planeCount() const
{
    return d->planeCount;
}

/*!
    Returns a type specific handle to a video frame's buffer.

    For an OpenGL texture this would be the texture ID.

    \sa QAbstractVideoBuffer::handle()
*/
QVariant QVideoFrame::handle() const
{
    return d->buffer != 0 ? d->buffer->handle() : QVariant();
}

/*!
    Returns the presentation time (in microseconds) when the frame should be displayed.

    An invalid time is represented as -1.

*/
qint64 QVideoFrame::startTime() const
{
    return d->startTime;
}

/*!
    Sets the presentation \a time (in microseconds) when the frame should initially be displayed.

    An invalid time is represented as -1.

*/
void QVideoFrame::setStartTime(qint64 time)
{
    d->startTime = time;
}

/*!
    Returns the presentation time (in microseconds) when a frame should stop being displayed.

    An invalid time is represented as -1.

*/
qint64 QVideoFrame::endTime() const
{
    return d->endTime;
}

/*!
    Sets the presentation \a time (in microseconds) when a frame should stop being displayed.

    An invalid time is represented as -1.

*/
void QVideoFrame::setEndTime(qint64 time)
{
    d->endTime = time;
}

/*!
    Returns any extra metadata associated with this frame.
 */
QVariantMap QVideoFrame::availableMetaData() const
{
    return d->metadata;
}

/*!
    Returns any metadata for this frame for the given \a key.

    This might include frame specific information from
    a camera, or subtitles from a decoded video stream.

    See the documentation for the relevant video frame
    producer for further information about available metadata.
 */
QVariant QVideoFrame::metaData(const QString &key) const
{
    return d->metadata.value(key);
}

/*!
    Sets the metadata for the given \a key to \a value.

    If \a value is a null variant, any metadata for this key will be removed.

    The producer of the video frame might use this to associate
    certain data with this frame, or for an intermediate processor
    to add information for a consumer of this frame.
 */
void QVideoFrame::setMetaData(const QString &key, const QVariant &value)
{
    if (!value.isNull())
        d->metadata.insert(key, value);
    else
        d->metadata.remove(key);
}

/*!
    Returns a video pixel format equivalent to an image \a format.  If there is no equivalent
    format QVideoFrame::InvalidType is returned instead.

    \note In general \l QImage does not handle YUV formats.

*/
QVideoFrame::PixelFormat QVideoFrame::pixelFormatFromImageFormat(QImage::Format format)
{
    switch (format) {
    case QImage::Format_RGB32:
    case QImage::Format_RGBX8888:
        return Format_RGB32;
    case QImage::Format_ARGB32:
    case QImage::Format_RGBA8888:
        return Format_ARGB32;
    case QImage::Format_ARGB32_Premultiplied:
    case QImage::Format_RGBA8888_Premultiplied:
        return Format_ARGB32_Premultiplied;
    case QImage::Format_RGB16:
        return Format_RGB565;
    case QImage::Format_ARGB8565_Premultiplied:
        return Format_ARGB8565_Premultiplied;
    case QImage::Format_RGB555:
        return Format_RGB555;
    case QImage::Format_RGB888:
        return Format_RGB24;
    default:
        return Format_Invalid;
    }
}

/*!
    Returns an image format equivalent to a video frame pixel \a format.  If there is no equivalent
    format QImage::Format_Invalid is returned instead.

    \note In general \l QImage does not handle YUV formats.

*/
QImage::Format QVideoFrame::imageFormatFromPixelFormat(PixelFormat format)
{
    switch (format) {
    case Format_Invalid:
        return QImage::Format_Invalid;
    case Format_ARGB32:
        return QImage::Format_ARGB32;
    case Format_ARGB32_Premultiplied:
        return QImage::Format_ARGB32_Premultiplied;
    case Format_RGB32:
        return QImage::Format_RGB32;
    case Format_RGB24:
        return QImage::Format_RGB888;
    case Format_RGB565:
        return QImage::Format_RGB16;
    case Format_RGB555:
        return QImage::Format_RGB555;
    case Format_ARGB8565_Premultiplied:
        return QImage::Format_ARGB8565_Premultiplied;
    case Format_BGRA32:
    case Format_BGRA32_Premultiplied:
    case Format_BGR32:
    case Format_BGR24:
        return QImage::Format_Invalid;
    case Format_BGR565:
    case Format_BGR555:
    case Format_BGRA5658_Premultiplied:
    case Format_AYUV444:
    case Format_AYUV444_Premultiplied:
    case Format_YUV444:
    case Format_YUV420P:
    case Format_YV12:
    case Format_UYVY:
    case Format_YUYV:
    case Format_NV12:
    case Format_NV21:
    case Format_IMC1:
    case Format_IMC2:
    case Format_IMC3:
    case Format_IMC4:
    case Format_Y8:
    case Format_Y16:
    case Format_Jpeg:
    case Format_CameraRaw:
    case Format_AdobeDng:
        return QImage::Format_Invalid;
    case Format_User:
    default:
        return QImage::Format_Invalid;
    }
    return QImage::Format_Invalid;
}


extern void QT_FASTCALL qt_convert_BGRA32_to_ARGB32(const QVideoFrame&, uchar*);
extern void QT_FASTCALL qt_convert_BGR24_to_ARGB32(const QVideoFrame&, uchar*);
extern void QT_FASTCALL qt_convert_BGR565_to_ARGB32(const QVideoFrame&, uchar*);
extern void QT_FASTCALL qt_convert_BGR555_to_ARGB32(const QVideoFrame&, uchar*);
extern void QT_FASTCALL qt_convert_AYUV444_to_ARGB32(const QVideoFrame&, uchar*);
extern void QT_FASTCALL qt_convert_YUV444_to_ARGB32(const QVideoFrame&, uchar*);
extern void QT_FASTCALL qt_convert_YUV420P_to_ARGB32(const QVideoFrame&, uchar*);
extern void QT_FASTCALL qt_convert_YV12_to_ARGB32(const QVideoFrame&, uchar*);
extern void QT_FASTCALL qt_convert_UYVY_to_ARGB32(const QVideoFrame&, uchar*);
extern void QT_FASTCALL qt_convert_YUYV_to_ARGB32(const QVideoFrame&, uchar*);
extern void QT_FASTCALL qt_convert_NV12_to_ARGB32(const QVideoFrame&, uchar*);
extern void QT_FASTCALL qt_convert_NV21_to_ARGB32(const QVideoFrame&, uchar*);

static VideoFrameConvertFunc qConvertFuncs[QVideoFrame::NPixelFormats] = {
    /* Format_Invalid */                Q_NULLPTR, // Not needed
    /* Format_ARGB32 */                 Q_NULLPTR, // Not needed
    /* Format_ARGB32_Premultiplied */   Q_NULLPTR, // Not needed
    /* Format_RGB32 */                  Q_NULLPTR, // Not needed
    /* Format_RGB24 */                  Q_NULLPTR, // Not needed
    /* Format_RGB565 */                 Q_NULLPTR, // Not needed
    /* Format_RGB555 */                 Q_NULLPTR, // Not needed
    /* Format_ARGB8565_Premultiplied */ Q_NULLPTR, // Not needed
    /* Format_BGRA32 */                 qt_convert_BGRA32_to_ARGB32,
    /* Format_BGRA32_Premultiplied */   qt_convert_BGRA32_to_ARGB32,
    /* Format_BGR32 */                  qt_convert_BGRA32_to_ARGB32,
    /* Format_BGR24 */                  qt_convert_BGR24_to_ARGB32,
    /* Format_BGR565 */                 qt_convert_BGR565_to_ARGB32,
    /* Format_BGR555 */                 qt_convert_BGR555_to_ARGB32,
    /* Format_BGRA5658_Premultiplied */ Q_NULLPTR,
    /* Format_AYUV444 */                qt_convert_AYUV444_to_ARGB32,
    /* Format_AYUV444_Premultiplied */  Q_NULLPTR,
    /* Format_YUV444 */                 qt_convert_YUV444_to_ARGB32,
    /* Format_YUV420P */                qt_convert_YUV420P_to_ARGB32,
    /* Format_YV12 */                   qt_convert_YV12_to_ARGB32,
    /* Format_UYVY */                   qt_convert_UYVY_to_ARGB32,
    /* Format_YUYV */                   qt_convert_YUYV_to_ARGB32,
    /* Format_NV12 */                   qt_convert_NV12_to_ARGB32,
    /* Format_NV21 */                   qt_convert_NV21_to_ARGB32,
    /* Format_IMC1 */                   Q_NULLPTR,
    /* Format_IMC2 */                   Q_NULLPTR,
    /* Format_IMC3 */                   Q_NULLPTR,
    /* Format_IMC4 */                   Q_NULLPTR,
    /* Format_Y8 */                     Q_NULLPTR,
    /* Format_Y16 */                    Q_NULLPTR,
    /* Format_Jpeg */                   Q_NULLPTR, // Not needed
    /* Format_CameraRaw */              Q_NULLPTR,
    /* Format_AdobeDng */               Q_NULLPTR
};

static void qInitConvertFuncsAsm()
{
#ifdef QT_COMPILER_SUPPORTS_SSE2
    extern void QT_FASTCALL qt_convert_BGRA32_to_ARGB32_sse2(const QVideoFrame&, uchar*);
    if (qCpuHasFeature(SSE2)){
        qConvertFuncs[QVideoFrame::Format_BGRA32] = qt_convert_BGRA32_to_ARGB32_sse2;
        qConvertFuncs[QVideoFrame::Format_BGRA32_Premultiplied] = qt_convert_BGRA32_to_ARGB32_sse2;
        qConvertFuncs[QVideoFrame::Format_BGR32] = qt_convert_BGRA32_to_ARGB32_sse2;
    }
#endif
#ifdef QT_COMPILER_SUPPORTS_SSSE3
    extern void QT_FASTCALL qt_convert_BGRA32_to_ARGB32_ssse3(const QVideoFrame&, uchar*);
    if (qCpuHasFeature(SSSE3)){
        qConvertFuncs[QVideoFrame::Format_BGRA32] = qt_convert_BGRA32_to_ARGB32_ssse3;
        qConvertFuncs[QVideoFrame::Format_BGRA32_Premultiplied] = qt_convert_BGRA32_to_ARGB32_ssse3;
        qConvertFuncs[QVideoFrame::Format_BGR32] = qt_convert_BGRA32_to_ARGB32_ssse3;
    }
#endif
#ifdef QT_COMPILER_SUPPORTS_AVX2
    extern void QT_FASTCALL qt_convert_BGRA32_to_ARGB32_avx2(const QVideoFrame&, uchar*);
    if (qCpuHasFeature(AVX2)){
        qConvertFuncs[QVideoFrame::Format_BGRA32] = qt_convert_BGRA32_to_ARGB32_avx2;
        qConvertFuncs[QVideoFrame::Format_BGRA32_Premultiplied] = qt_convert_BGRA32_to_ARGB32_avx2;
        qConvertFuncs[QVideoFrame::Format_BGR32] = qt_convert_BGRA32_to_ARGB32_avx2;
    }
#endif
}

/*!
    \internal
*/
QImage qt_imageFromVideoFrame(const QVideoFrame &f)
{
    QVideoFrame &frame = const_cast<QVideoFrame&>(f);
    QImage result;

    if (!frame.isValid() || !frame.map(QAbstractVideoBuffer::ReadOnly))
        return result;

    // Formats supported by QImage don't need conversion
    QImage::Format imageFormat = QVideoFrame::imageFormatFromPixelFormat(frame.pixelFormat());
    if (imageFormat != QImage::Format_Invalid) {
        result = QImage(frame.bits(), frame.width(), frame.height(), imageFormat).copy();
    }

    // Load from JPG
    else if (frame.pixelFormat() == QVideoFrame::Format_Jpeg) {
        result.loadFromData(frame.bits(), frame.mappedBytes(), "JPG");
    }

    // Need conversion
    else {
        static bool initAsmFuncsDone = false;
        if (!initAsmFuncsDone) {
            qInitConvertFuncsAsm();
            initAsmFuncsDone = true;
        }
        VideoFrameConvertFunc convert = qConvertFuncs[frame.pixelFormat()];
        if (!convert) {
            qWarning() << Q_FUNC_INFO << ": unsupported pixel format" << frame.pixelFormat();
        } else {
            result = QImage(frame.width(), frame.height(), QImage::Format_ARGB32);
            convert(frame, result.bits());
        }
    }

    frame.unmap();

    return result;
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, QVideoFrame::PixelFormat pf)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (pf) {
        case QVideoFrame::Format_Invalid:
            return dbg << "Format_Invalid";
        case QVideoFrame::Format_ARGB32:
            return dbg << "Format_ARGB32";
        case QVideoFrame::Format_ARGB32_Premultiplied:
            return dbg << "Format_ARGB32_Premultiplied";
        case QVideoFrame::Format_RGB32:
            return dbg << "Format_RGB32";
        case QVideoFrame::Format_RGB24:
            return dbg << "Format_RGB24";
        case QVideoFrame::Format_RGB565:
            return dbg << "Format_RGB565";
        case QVideoFrame::Format_RGB555:
            return dbg << "Format_RGB555";
        case QVideoFrame::Format_ARGB8565_Premultiplied:
            return dbg << "Format_ARGB8565_Premultiplied";
        case QVideoFrame::Format_BGRA32:
            return dbg << "Format_BGRA32";
        case QVideoFrame::Format_BGRA32_Premultiplied:
            return dbg << "Format_BGRA32_Premultiplied";
        case QVideoFrame::Format_BGR32:
            return dbg << "Format_BGR32";
        case QVideoFrame::Format_BGR24:
            return dbg << "Format_BGR24";
        case QVideoFrame::Format_BGR565:
            return dbg << "Format_BGR565";
        case QVideoFrame::Format_BGR555:
            return dbg << "Format_BGR555";
        case QVideoFrame::Format_BGRA5658_Premultiplied:
            return dbg << "Format_BGRA5658_Premultiplied";
        case QVideoFrame::Format_AYUV444:
            return dbg << "Format_AYUV444";
        case QVideoFrame::Format_AYUV444_Premultiplied:
            return dbg << "Format_AYUV444_Premultiplied";
        case QVideoFrame::Format_YUV444:
            return dbg << "Format_YUV444";
        case QVideoFrame::Format_YUV420P:
            return dbg << "Format_YUV420P";
        case QVideoFrame::Format_YV12:
            return dbg << "Format_YV12";
        case QVideoFrame::Format_UYVY:
            return dbg << "Format_UYVY";
        case QVideoFrame::Format_YUYV:
            return dbg << "Format_YUYV";
        case QVideoFrame::Format_NV12:
            return dbg << "Format_NV12";
        case QVideoFrame::Format_NV21:
            return dbg << "Format_NV21";
        case QVideoFrame::Format_IMC1:
            return dbg << "Format_IMC1";
        case QVideoFrame::Format_IMC2:
            return dbg << "Format_IMC2";
        case QVideoFrame::Format_IMC3:
            return dbg << "Format_IMC3";
        case QVideoFrame::Format_IMC4:
            return dbg << "Format_IMC4";
        case QVideoFrame::Format_Y8:
            return dbg << "Format_Y8";
        case QVideoFrame::Format_Y16:
            return dbg << "Format_Y16";
        case QVideoFrame::Format_Jpeg:
            return dbg << "Format_Jpeg";
        case QVideoFrame::Format_AdobeDng:
            return dbg << "Format_AdobeDng";
        case QVideoFrame::Format_CameraRaw:
            return dbg << "Format_CameraRaw";

        default:
            return dbg << QString(QLatin1String("UserType(%1)" )).arg(int(pf)).toLatin1().constData();
    }
}

QDebug operator<<(QDebug dbg, QVideoFrame::FieldType f)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (f) {
        case QVideoFrame::TopField:
            return dbg << "TopField";
        case QVideoFrame::BottomField:
            return dbg << "BottomField";
        case QVideoFrame::InterlacedFrame:
            return dbg << "InterlacedFrame";
        default:
            return dbg << "ProgressiveFrame";
    }
}

static QString qFormatTimeStamps(qint64 start, qint64 end)
{
    // Early out for invalid.
    if (start < 0)
        return QLatin1String("[no timestamp]");

    bool onlyOne = (start == end);

    // [hh:]mm:ss.ms
    const int s_millis = start % 1000000;
    start /= 1000000;
    const int s_seconds = start % 60;
    start /= 60;
    const int s_minutes = start % 60;
    start /= 60;

    if (onlyOne) {
        if (start > 0)
            return QString::fromLatin1("@%1:%2:%3.%4")
                    .arg(start, 1, 10, QLatin1Char('0'))
                    .arg(s_minutes, 2, 10, QLatin1Char('0'))
                    .arg(s_seconds, 2, 10, QLatin1Char('0'))
                    .arg(s_millis, 2, 10, QLatin1Char('0'));
        else
            return QString::fromLatin1("@%1:%2.%3")
                    .arg(s_minutes, 2, 10, QLatin1Char('0'))
                    .arg(s_seconds, 2, 10, QLatin1Char('0'))
                    .arg(s_millis, 2, 10, QLatin1Char('0'));
    } else if (end == -1) {
        // Similar to start-start, except it means keep displaying it?
        if (start > 0)
            return QString::fromLatin1("%1:%2:%3.%4 - forever")
                    .arg(start, 1, 10, QLatin1Char('0'))
                    .arg(s_minutes, 2, 10, QLatin1Char('0'))
                    .arg(s_seconds, 2, 10, QLatin1Char('0'))
                    .arg(s_millis, 2, 10, QLatin1Char('0'));
        else
            return QString::fromLatin1("%1:%2.%3 - forever")
                    .arg(s_minutes, 2, 10, QLatin1Char('0'))
                    .arg(s_seconds, 2, 10, QLatin1Char('0'))
                    .arg(s_millis, 2, 10, QLatin1Char('0'));
    } else {
        const int e_millis = end % 1000000;
        end /= 1000000;
        const int e_seconds = end % 60;
        end /= 60;
        const int e_minutes = end % 60;
        end /= 60;

        if (start > 0 || end > 0)
            return QString::fromLatin1("%1:%2:%3.%4 - %5:%6:%7.%8")
                    .arg(start, 1, 10, QLatin1Char('0'))
                    .arg(s_minutes, 2, 10, QLatin1Char('0'))
                    .arg(s_seconds, 2, 10, QLatin1Char('0'))
                    .arg(s_millis, 2, 10, QLatin1Char('0'))
                    .arg(end, 1, 10, QLatin1Char('0'))
                    .arg(e_minutes, 2, 10, QLatin1Char('0'))
                    .arg(e_seconds, 2, 10, QLatin1Char('0'))
                    .arg(e_millis, 2, 10, QLatin1Char('0'));
        else
            return QString::fromLatin1("%1:%2.%3 - %4:%5.%6")
                    .arg(s_minutes, 2, 10, QLatin1Char('0'))
                    .arg(s_seconds, 2, 10, QLatin1Char('0'))
                    .arg(s_millis, 2, 10, QLatin1Char('0'))
                    .arg(e_minutes, 2, 10, QLatin1Char('0'))
                    .arg(e_seconds, 2, 10, QLatin1Char('0'))
                    .arg(e_millis, 2, 10, QLatin1Char('0'));
    }
}

QDebug operator<<(QDebug dbg, const QVideoFrame& f)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "QVideoFrame(" << f.size() << ", "
               << f.pixelFormat() << ", "
               << f.handleType() << ", "
               << f.mapMode() << ", "
               << qFormatTimeStamps(f.startTime(), f.endTime()).toLatin1().constData();
    if (f.availableMetaData().count())
        dbg << ", metaData: " << f.availableMetaData();
    dbg << ')';
    return dbg;
}
#endif

QT_END_NAMESPACE

