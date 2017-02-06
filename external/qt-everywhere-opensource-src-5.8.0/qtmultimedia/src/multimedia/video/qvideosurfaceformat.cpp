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

#include "qvideosurfaceformat.h"

#include <qdebug.h>
#include <qmetatype.h>
#include <qpair.h>
#include <qvariant.h>
#include <qvector.h>

QT_BEGIN_NAMESPACE

static void qRegisterVideoSurfaceFormatMetaTypes()
{
    qRegisterMetaType<QVideoSurfaceFormat>();
    qRegisterMetaType<QVideoSurfaceFormat::Direction>();
    qRegisterMetaType<QVideoSurfaceFormat::YCbCrColorSpace>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterVideoSurfaceFormatMetaTypes)


class QVideoSurfaceFormatPrivate : public QSharedData
{
public:
    QVideoSurfaceFormatPrivate()
        : pixelFormat(QVideoFrame::Format_Invalid)
        , handleType(QAbstractVideoBuffer::NoHandle)
        , scanLineDirection(QVideoSurfaceFormat::TopToBottom)
        , pixelAspectRatio(1, 1)
        , ycbcrColorSpace(QVideoSurfaceFormat::YCbCr_Undefined)
        , frameRate(0.0)
        , mirrored(false)
    {
    }

    QVideoSurfaceFormatPrivate(
            const QSize &size,
            QVideoFrame::PixelFormat format,
            QAbstractVideoBuffer::HandleType type)
        : pixelFormat(format)
        , handleType(type)
        , scanLineDirection(QVideoSurfaceFormat::TopToBottom)
        , frameSize(size)
        , pixelAspectRatio(1, 1)
        , ycbcrColorSpace(QVideoSurfaceFormat::YCbCr_Undefined)
        , viewport(QPoint(0, 0), size)
        , frameRate(0.0)
        , mirrored(false)
    {
    }

    QVideoSurfaceFormatPrivate(const QVideoSurfaceFormatPrivate &other)
        : QSharedData(other)
        , pixelFormat(other.pixelFormat)
        , handleType(other.handleType)
        , scanLineDirection(other.scanLineDirection)
        , frameSize(other.frameSize)
        , pixelAspectRatio(other.pixelAspectRatio)
        , ycbcrColorSpace(other.ycbcrColorSpace)
        , viewport(other.viewport)
        , frameRate(other.frameRate)
        , mirrored(other.mirrored)
        , propertyNames(other.propertyNames)
        , propertyValues(other.propertyValues)
    {
    }

    bool operator ==(const QVideoSurfaceFormatPrivate &other) const
    {
        if (pixelFormat == other.pixelFormat
            && handleType == other.handleType
            && scanLineDirection == other.scanLineDirection
            && frameSize == other.frameSize
            && pixelAspectRatio == other.pixelAspectRatio
            && viewport == other.viewport
            && frameRatesEqual(frameRate, other.frameRate)
            && ycbcrColorSpace == other.ycbcrColorSpace
            && mirrored == other.mirrored
            && propertyNames.count() == other.propertyNames.count()) {
            for (int i = 0; i < propertyNames.count(); ++i) {
                int j = other.propertyNames.indexOf(propertyNames.at(i));

                if (j == -1 || propertyValues.at(i) != other.propertyValues.at(j))
                    return false;
            }
            return true;
        } else {
            return false;
        }
    }

    inline static bool frameRatesEqual(qreal r1, qreal r2)
    {
        return qAbs(r1 - r2) <= 0.00001 * qMin(qAbs(r1), qAbs(r2));
    }

    QVideoFrame::PixelFormat pixelFormat;
    QAbstractVideoBuffer::HandleType handleType;
    QVideoSurfaceFormat::Direction scanLineDirection;
    QSize frameSize;
    QSize pixelAspectRatio;
    QVideoSurfaceFormat::YCbCrColorSpace ycbcrColorSpace;
    QRect viewport;
    qreal frameRate;
    bool mirrored;
    QList<QByteArray> propertyNames;
    QList<QVariant> propertyValues;
};

/*!
    \class QVideoSurfaceFormat
    \brief The QVideoSurfaceFormat class specifies the stream format of a video presentation
    surface.
    \inmodule QtMultimedia

    \ingroup multimedia
    \ingroup multimedia_video

    A video surface presents a stream of video frames.  The surface's format describes the type of
    the frames and determines how they should be presented.

    The core properties of a video stream required to setup a video surface are the pixel format
    given by pixelFormat(), and the frame dimensions given by frameSize().

    If the surface is to present frames using a frame's handle a surface format will also include
    a handle type which is given by the handleType() function.

    The region of a frame that is actually displayed on a video surface is given by the viewport().
    A stream may have a viewport less than the entire region of a frame to allow for videos smaller
    than the nearest optimal size of a video frame.  For example the width of a frame may be
    extended so that the start of each scan line is eight byte aligned.

    Other common properties are the pixelAspectRatio(), scanLineDirection(), and frameRate().
    Additionally a stream may have some additional type specific properties which are listed by the
    dynamicPropertyNames() function and can be accessed using the property(), and setProperty()
    functions.
*/

/*!
    \enum QVideoSurfaceFormat::Direction

    Enumerates the layout direction of video scan lines.

    \value TopToBottom Scan lines are arranged from the top of the frame to the bottom.
    \value BottomToTop Scan lines are arranged from the bottom of the frame to the top.
*/

/*!
    \enum QVideoSurfaceFormat::YCbCrColorSpace

    Enumerates the Y'CbCr color space of video frames.

    \value YCbCr_Undefined
    No color space is specified.

    \value YCbCr_BT601
    A Y'CbCr color space defined by ITU-R recommendation BT.601
    with Y value range from 16 to 235, and Cb/Cr range from 16 to 240.
    Used in standard definition video.

    \value YCbCr_BT709
    A Y'CbCr color space defined by ITU-R BT.709 with the same values range as YCbCr_BT601.  Used
    for HDTV.

    \value YCbCr_xvYCC601
    The BT.601 color space with the value range extended to 0 to 255.
    It is backward compatibile with BT.601 and uses values outside BT.601 range to represent a
    wider range of colors.

    \value YCbCr_xvYCC709
    The BT.709 color space with the value range extended to 0 to 255.

    \value YCbCr_JPEG
    The full range Y'CbCr color space used in JPEG files.
*/

/*!
    Constructs a null video stream format.
*/
QVideoSurfaceFormat::QVideoSurfaceFormat()
    : d(new QVideoSurfaceFormatPrivate)
{
}

/*!
    Contructs a description of stream which receives stream of \a type buffers with given frame
    \a size and pixel \a format.
*/
QVideoSurfaceFormat::QVideoSurfaceFormat(
        const QSize& size, QVideoFrame::PixelFormat format, QAbstractVideoBuffer::HandleType type)
    : d(new QVideoSurfaceFormatPrivate(size, format, type))
{
}

/*!
    Constructs a copy of \a other.
*/
QVideoSurfaceFormat::QVideoSurfaceFormat(const QVideoSurfaceFormat &other)
    : d(other.d)
{
}

/*!
    Assigns the values of \a other to this object.
*/
QVideoSurfaceFormat &QVideoSurfaceFormat::operator =(const QVideoSurfaceFormat &other)
{
    d = other.d;

    return *this;
}

/*!
    Destroys a video stream description.
*/
QVideoSurfaceFormat::~QVideoSurfaceFormat()
{
}

/*!
    Identifies if a video surface format has a valid pixel format and frame size.

    Returns true if the format is valid, and false otherwise.
*/
bool QVideoSurfaceFormat::isValid() const
{
    return d->pixelFormat != QVideoFrame::Format_Invalid && d->frameSize.isValid();
}

/*!
    Returns true if \a other is the same as this video format, and false if they are different.
*/
bool QVideoSurfaceFormat::operator ==(const QVideoSurfaceFormat &other) const
{
    return d == other.d || *d == *other.d;
}

/*!
    Returns true if \a other is different to this video format, and false if they are the same.
*/
bool QVideoSurfaceFormat::operator !=(const QVideoSurfaceFormat &other) const
{
    return d != other.d && !(*d == *other.d);
}

/*!
    Returns the pixel format of frames in a video stream.
*/
QVideoFrame::PixelFormat QVideoSurfaceFormat::pixelFormat() const
{
    return d->pixelFormat;
}

/*!
    Returns the type of handle the surface uses to present the frame data.

    If the handle type is \c QAbstractVideoBuffer::NoHandle, buffers with any handle type are valid
    provided they can be \l {QAbstractVideoBuffer::map()}{mapped} with the
    QAbstractVideoBuffer::ReadOnly flag.  If the handleType() is not QAbstractVideoBuffer::NoHandle
    then the handle type of the buffer must be the same as that of the surface format.
*/
QAbstractVideoBuffer::HandleType QVideoSurfaceFormat::handleType() const
{
    return d->handleType;
}

/*!
    Returns the dimensions of frames in a video stream.

    \sa frameWidth(), frameHeight()
*/
QSize QVideoSurfaceFormat::frameSize() const
{
    return d->frameSize;
}

/*!
    Returns the width of frames in a video stream.

    \sa frameSize(), frameHeight()
*/
int QVideoSurfaceFormat::frameWidth() const
{
    return d->frameSize.width();
}

/*!
    Returns the height of frame in a video stream.
*/
int QVideoSurfaceFormat::frameHeight() const
{
    return d->frameSize.height();
}

/*!
    Sets the size of frames in a video stream to \a size.

    This will reset the viewport() to fill the entire frame.
*/
void QVideoSurfaceFormat::setFrameSize(const QSize &size)
{
    d->frameSize = size;
    d->viewport = QRect(QPoint(0, 0), size);
}

/*!
    \overload

    Sets the \a width and \a height of frames in a video stream.

    This will reset the viewport() to fill the entire frame.
*/
void QVideoSurfaceFormat::setFrameSize(int width, int height)
{
    d->frameSize = QSize(width, height);
    d->viewport = QRect(0, 0, width, height);
}

/*!
    Returns the viewport of a video stream.

    The viewport is the region of a video frame that is actually displayed.

    By default the viewport covers an entire frame.
*/
QRect QVideoSurfaceFormat::viewport() const
{
    return d->viewport;
}

/*!
    Sets the viewport of a video stream to \a viewport.
*/
void QVideoSurfaceFormat::setViewport(const QRect &viewport)
{
    d->viewport = viewport;
}

/*!
    Returns the direction of scan lines.
*/
QVideoSurfaceFormat::Direction QVideoSurfaceFormat::scanLineDirection() const
{
    return d->scanLineDirection;
}

/*!
    Sets the \a direction of scan lines.
*/
void QVideoSurfaceFormat::setScanLineDirection(Direction direction)
{
    d->scanLineDirection = direction;
}

/*!
    Returns the frame rate of a video stream in frames per second.
*/
qreal QVideoSurfaceFormat::frameRate() const
{
    return d->frameRate;
}

/*!
    Sets the frame \a rate of a video stream in frames per second.
*/
void QVideoSurfaceFormat::setFrameRate(qreal rate)
{
    d->frameRate = rate;
}

/*!
    Returns a video stream's pixel aspect ratio.
*/
QSize QVideoSurfaceFormat::pixelAspectRatio() const
{
    return d->pixelAspectRatio;
}

/*!
    Sets a video stream's pixel aspect \a ratio.
*/
void QVideoSurfaceFormat::setPixelAspectRatio(const QSize &ratio)
{
    d->pixelAspectRatio = ratio;
}

/*!
    \overload

    Sets the \a horizontal and \a vertical elements of a video stream's pixel aspect ratio.
*/
void QVideoSurfaceFormat::setPixelAspectRatio(int horizontal, int vertical)
{
    d->pixelAspectRatio = QSize(horizontal, vertical);
}

/*!
    Returns the Y'CbCr color space of a video stream.
*/
QVideoSurfaceFormat::YCbCrColorSpace QVideoSurfaceFormat::yCbCrColorSpace() const
{
    return d->ycbcrColorSpace;
}

/*!
    Sets the Y'CbCr color \a space of a video stream.
    It is only used with raw YUV frame types.
*/
void QVideoSurfaceFormat::setYCbCrColorSpace(QVideoSurfaceFormat::YCbCrColorSpace space)
{
    d->ycbcrColorSpace = space;
}

/*!
    Returns a suggested size in pixels for the video stream.

    This is the size of the viewport scaled according to the pixel aspect ratio.
*/
QSize QVideoSurfaceFormat::sizeHint() const
{
    QSize size = d->viewport.size();

    if (d->pixelAspectRatio.height() != 0)
        size.setWidth(size.width() * d->pixelAspectRatio.width() / d->pixelAspectRatio.height());

    return size;
}

/*!
    Returns a list of video format dynamic property names.
*/
QList<QByteArray> QVideoSurfaceFormat::propertyNames() const
{
    return (QList<QByteArray>()
            << "handleType"
            << "pixelFormat"
            << "frameSize"
            << "frameWidth"
            << "viewport"
            << "scanLineDirection"
            << "frameRate"
            << "pixelAspectRatio"
            << "sizeHint"
            << "yCbCrColorSpace"
            << "mirrored")
            + d->propertyNames;
}

/*!
    Returns the value of the video format's \a name property.
*/
QVariant QVideoSurfaceFormat::property(const char *name) const
{
    if (qstrcmp(name, "handleType") == 0) {
        return qVariantFromValue(d->handleType);
    } else if (qstrcmp(name, "pixelFormat") == 0) {
        return qVariantFromValue(d->pixelFormat);
    } else if (qstrcmp(name, "frameSize") == 0) {
        return d->frameSize;
    } else if (qstrcmp(name, "frameWidth") == 0) {
        return d->frameSize.width();
    } else if (qstrcmp(name, "frameHeight") == 0) {
        return d->frameSize.height();
    } else if (qstrcmp(name, "viewport") == 0) {
        return d->viewport;
    } else if (qstrcmp(name, "scanLineDirection") == 0) {
        return qVariantFromValue(d->scanLineDirection);
    } else if (qstrcmp(name, "frameRate") == 0) {
        return qVariantFromValue(d->frameRate);
    } else if (qstrcmp(name, "pixelAspectRatio") == 0) {
        return qVariantFromValue(d->pixelAspectRatio);
    } else if (qstrcmp(name, "sizeHint") == 0) {
        return sizeHint();
    } else if (qstrcmp(name, "yCbCrColorSpace") == 0) {
        return qVariantFromValue(d->ycbcrColorSpace);
    } else if (qstrcmp(name, "mirrored") == 0) {
        return d->mirrored;
    } else {
        int id = 0;
        for (; id < d->propertyNames.count() && d->propertyNames.at(id) != name; ++id) {}

        return id < d->propertyValues.count()
                ? d->propertyValues.at(id)
                : QVariant();
    }
}

/*!
    Sets the video format's \a name property to \a value.

    Trying to set a read only property will be ignored.

*/
void QVideoSurfaceFormat::setProperty(const char *name, const QVariant &value)
{
    if (qstrcmp(name, "handleType") == 0) {
        // read only.
    } else if (qstrcmp(name, "pixelFormat") == 0) {
        // read only.
    } else if (qstrcmp(name, "frameSize") == 0) {
        if (value.canConvert<QSize>()) {
            d->frameSize = qvariant_cast<QSize>(value);
            d->viewport = QRect(QPoint(0, 0), d->frameSize);
        }
    } else if (qstrcmp(name, "frameWidth") == 0) {
        // read only.
    } else if (qstrcmp(name, "frameHeight") == 0) {
        // read only.
    } else if (qstrcmp(name, "viewport") == 0) {
        if (value.canConvert<QRect>())
            d->viewport = qvariant_cast<QRect>(value);
    } else if (qstrcmp(name, "scanLineDirection") == 0) {
        if (value.canConvert<Direction>())
            d->scanLineDirection = qvariant_cast<Direction>(value);
    } else if (qstrcmp(name, "frameRate") == 0) {
        if (value.canConvert<qreal>())
            d->frameRate = qvariant_cast<qreal>(value);
    } else if (qstrcmp(name, "pixelAspectRatio") == 0) {
        if (value.canConvert<QSize>())
            d->pixelAspectRatio = qvariant_cast<QSize>(value);
    } else if (qstrcmp(name, "sizeHint") == 0) {
        // read only.
    } else if (qstrcmp(name, "yCbCrColorSpace") == 0) {
          if (value.canConvert<YCbCrColorSpace>())
              d->ycbcrColorSpace = qvariant_cast<YCbCrColorSpace>(value);
    } else if (qstrcmp(name, "mirrored") == 0) {
        if (value.canConvert<bool>())
            d->mirrored = qvariant_cast<bool>(value);
    } else {
        int id = 0;
        for (; id < d->propertyNames.count() && d->propertyNames.at(id) != name; ++id) {}

        if (id < d->propertyValues.count()) {
            if (value.isNull()) {
                d->propertyNames.removeAt(id);
                d->propertyValues.removeAt(id);
            } else {
                d->propertyValues[id] = value;
            }
        } else if (!value.isNull()) {
            d->propertyNames.append(QByteArray(name));
            d->propertyValues.append(value);
        }
    }
}


#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, QVideoSurfaceFormat::YCbCrColorSpace cs)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (cs) {
        case QVideoSurfaceFormat::YCbCr_BT601:
            dbg << "YCbCr_BT601";
            break;
        case QVideoSurfaceFormat::YCbCr_BT709:
            dbg << "YCbCr_BT709";
            break;
        case QVideoSurfaceFormat::YCbCr_JPEG:
            dbg << "YCbCr_JPEG";
            break;
        case QVideoSurfaceFormat::YCbCr_xvYCC601:
            dbg << "YCbCr_xvYCC601";
            break;
        case QVideoSurfaceFormat::YCbCr_xvYCC709:
            dbg << "YCbCr_xvYCC709";
            break;
        case QVideoSurfaceFormat::YCbCr_CustomMatrix:
            dbg << "YCbCr_CustomMatrix";
            break;
        default:
            dbg << "YCbCr_Undefined";
            break;
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, QVideoSurfaceFormat::Direction dir)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (dir) {
        case QVideoSurfaceFormat::BottomToTop:
            dbg << "BottomToTop";
            break;
        case QVideoSurfaceFormat::TopToBottom:
            dbg << "TopToBottom";
            break;
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, const QVideoSurfaceFormat &f)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "QVideoSurfaceFormat(" << f.pixelFormat() << ", " << f.frameSize()
        << ", viewport=" << f.viewport() << ", pixelAspectRatio=" << f.pixelAspectRatio()
        << ", handleType=" << f.handleType() <<  ", yCbCrColorSpace=" << f.yCbCrColorSpace()
        << ')';

    const auto propertyNames = f.propertyNames();
    for (const QByteArray& propertyName : propertyNames)
        dbg << "\n    " << propertyName.data() << " = " << f.property(propertyName.data());

    return dbg;
}
#endif

QT_END_NAMESPACE
