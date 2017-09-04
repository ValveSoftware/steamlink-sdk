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

//TESTED_COMPONENT=src/multimedia

#include "qabstractvideosurface.h"

#include "qvideosurfaceformat.h"

#include <QtCore/qvariant.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

static void qRegisterAbstractVideoSurfaceMetaTypes()
{
    qRegisterMetaType<QAbstractVideoSurface::Error>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterAbstractVideoSurfaceMetaTypes)


class QAbstractVideoSurfacePrivate {
public:
    QAbstractVideoSurfacePrivate()
        : error(QAbstractVideoSurface::NoError),
          active(false)
    {
    }

public:
    QVideoSurfaceFormat surfaceFormat;
    QAbstractVideoSurface::Error error;
    QSize nativeResolution;
    bool active;
};

/*!
    \class QAbstractVideoSurface
    \brief The QAbstractVideoSurface class is a base class for video presentation surfaces.
    \inmodule QtMultimedia

    \ingroup multimedia
    \ingroup multimedia_video

    The QAbstractVideoSurface class defines the standard interface that video producers use to
    inter-operate with video presentation surfaces.  You can subclass this interface to receive
    video frames from sources like \l {QMediaPlayer}{decoded media} or \l {QCamera}{cameras} to
    perform your own processing.

    A video surface presents a continuous stream of identically formatted QVideoFrame instances, where the format
    of each frame is compatible with a stream format supplied when starting a presentation.  Each frame
    may have timestamp information that can be used by the surface to decide when to display that
    frame.

    A list of pixel formats a surface can present is given by the supportedPixelFormats() function,
    and the isFormatSupported() function will test if a video surface format is supported.  If a
    format is not supported the nearestFormat() function may be able to suggest a similar format.
    For example, if a surface supports fixed set of resolutions it may suggest the smallest
    supported resolution that contains the proposed resolution.

    The start() function takes a supported format and enables a video surface.  Once started a
    surface will begin displaying the frames it receives in the present() function.  Surfaces may
    hold a reference to the buffer of a presented video frame until a new frame is presented or
    streaming is stopped.  In addition, a video surface may hold a reference to a video frame
    until the \l {QVideoFrame::endTime()}{end timestamp} has passed.  The stop() function will
    disable a surface and release any video buffers it holds references to.

    \section2 Implementing a subclass of QAbstractVideoSurface

    When implementing a subclass of this interface, there are only a handful of functions to
    implement, broken down into two classes:

    \list
    \li Format related
    \li Presentation related
    \endlist

    For format related functionality, you just have to describe the pixel formats that you
    support (and the nearestFormat() function).  For presentation related functionality, you
    have to implement the present() function, and the start() and stop() functions.

    \note You must call the base class implementation of start() and stop() in your implementation.
*/

/*!
    \enum QAbstractVideoSurface::Error
    This enum describes the errors that may be returned by the error() function.

    \value NoError No error occurred.
    \value UnsupportedFormatError A video format was not supported.
    \value IncorrectFormatError A video frame was not compatible with the format of the surface.
    \value StoppedError The surface has not been started.
    \value ResourceError The surface could not allocate some resource.
*/

/*!
    Constructs a video surface with the given \a parent.
*/
QAbstractVideoSurface::QAbstractVideoSurface(QObject *parent)
    : QObject(parent),
      d_ptr(new QAbstractVideoSurfacePrivate)
{
}

/*!
    Destroys a video surface.
*/
QAbstractVideoSurface::~QAbstractVideoSurface()
{
}

/*!
    \fn QAbstractVideoSurface::supportedPixelFormats(QAbstractVideoBuffer::HandleType type) const

    Returns a list of pixel formats a video surface can present for a given handle \a type.

    The pixel formats returned for the QAbstractVideoBuffer::NoHandle type are valid for any buffer
    that can be mapped in read-only mode.

    Types that are first in the list can be assumed to be faster to render.
*/

/*!
    Tests a video surface \a format to determine if a surface can accept it.

    Returns true if the format is supported by the surface, and false otherwise.
*/
bool QAbstractVideoSurface::isFormatSupported(const QVideoSurfaceFormat &format) const
{
    return supportedPixelFormats(format.handleType()).contains(format.pixelFormat());
}

/*!
    Returns a supported video surface format that is similar to \a format.

    A similar surface format is one that has the same \l {QVideoSurfaceFormat::pixelFormat()}{pixel
    format} and \l {QVideoSurfaceFormat::handleType()}{handle type} but may differ in some of the other
    properties.  For example, if there are restrictions on the \l {QVideoSurfaceFormat::frameSize()}
    {frame sizes} a video surface can accept it may suggest a format with a larger frame size and
    a \l {QVideoSurfaceFormat::viewport()}{viewport} the size of the original frame size.

    If the format is already supported it will be returned unchanged, or if there is no similar
    supported format an invalid format will be returned.
*/
QVideoSurfaceFormat QAbstractVideoSurface::nearestFormat(const QVideoSurfaceFormat &format) const
{
    return isFormatSupported(format)
            ? format
            : QVideoSurfaceFormat();
}

/*!
    \fn QAbstractVideoSurface::supportedFormatsChanged()

    Signals that the set of formats supported by a video surface has changed.

    \sa supportedPixelFormats(), isFormatSupported()
*/

/*!
    Returns the format of a video surface.
*/
QVideoSurfaceFormat QAbstractVideoSurface::surfaceFormat() const
{
    Q_D(const QAbstractVideoSurface);
    return d->surfaceFormat;
}

/*!
    \fn QAbstractVideoSurface::surfaceFormatChanged(const QVideoSurfaceFormat &format)

    Signals that the configured \a format of a video surface has changed.

    \sa surfaceFormat(), start()
*/

/*!
    Starts a video surface presenting \a format frames.

    Returns true if the surface was started, and false if an error occurred.

    \note You must call the base class implementation of start() at the end of your implementation.
    \sa isActive(), stop()
*/
bool QAbstractVideoSurface::start(const QVideoSurfaceFormat &format)
{
    Q_D(QAbstractVideoSurface);
    bool wasActive  = d->active;

    d->active = true;
    d->surfaceFormat = format;
    d->error = NoError;

    emit surfaceFormatChanged(format);

    if (!wasActive)
        emit activeChanged(true);

    return true;
}

/*!
    Stops a video surface presenting frames and releases any resources acquired in start().

    \note You must call the base class implementation of stop() at the start of your implementation.
    \sa isActive(), start()
*/
void QAbstractVideoSurface::stop()
{
    Q_D(QAbstractVideoSurface);
    if (d->active) {
        d->surfaceFormat = QVideoSurfaceFormat();
        d->active = false;

        emit activeChanged(false);
        emit surfaceFormatChanged(surfaceFormat());
    }
}

/*!
    Indicates whether a video surface has been started.

    Returns true if the surface has been started, and false otherwise.
*/
bool QAbstractVideoSurface::isActive() const
{
    Q_D(const QAbstractVideoSurface);
    return d->active;
}

/*!
    \fn QAbstractVideoSurface::activeChanged(bool active)

    Signals that the \a active state of a video surface has changed.

    \sa isActive(), start(), stop()
*/

/*!
    \fn QAbstractVideoSurface::present(const QVideoFrame &frame)

    Presents a video \a frame.

    Returns true if the frame was presented, and false if an error occurred.

    Not all surfaces will block until the presentation of a frame has completed.  Calling present()
    on a non-blocking surface may fail if called before the presentation of a previous frame has
    completed.  In such cases the surface may not return to a ready state until it has had an
    opportunity to process events.

    If present() fails for any other reason the surface should immediately enter the stopped state
    and an error() value will be set.

    A video surface must be in the started state for present() to succeed, and the format of the
    video frame must be compatible with the current video surface format.

    \sa error()
*/

/*!
    Returns the last error that occurred.

    If a surface fails to start(), or stops unexpectedly this function can be called to discover
    what error occurred.
*/

QAbstractVideoSurface::Error QAbstractVideoSurface::error() const
{
    Q_D(const QAbstractVideoSurface);
    return d->error;
}

/*!
    Sets the value of error() to \a error.

    This can be called by implementors of this interface to communicate
    what the most recent error was.
*/
void QAbstractVideoSurface::setError(Error error)
{
    Q_D(QAbstractVideoSurface);
    d->error = error;
}

/*!
   \property QAbstractVideoSurface::nativeResolution

   The native resolution of video surface.
   This is the resolution of video frames the surface
   can render with optimal quality and/or performance.

   The native resolution is not always known and can be changed during playback.
 */
QSize QAbstractVideoSurface::nativeResolution() const
{
    Q_D(const QAbstractVideoSurface);
    return d->nativeResolution;
}

/*!
    Set the video surface native \a resolution.

    This function can be called by implementors of this interface to specify
    to frame producers what the native resolution of this surface is.
 */
void QAbstractVideoSurface::setNativeResolution(const QSize &resolution)
{
    Q_D(QAbstractVideoSurface);

    if (d->nativeResolution != resolution) {
        d->nativeResolution = resolution;

        emit nativeResolutionChanged(resolution);
    }
}
/*!
    \fn QAbstractVideoSurface::nativeResolutionChanged(const QSize &resolution);

    Signals the native \a resolution of video surface has changed.
*/

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QAbstractVideoSurface::Error& error)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (error) {
    case QAbstractVideoSurface::UnsupportedFormatError:
        dbg << "UnsupportedFormatError";
        break;
    case QAbstractVideoSurface::IncorrectFormatError:
        dbg << "IncorrectFormatError";
        break;
    case QAbstractVideoSurface::StoppedError:
        dbg << "StoppedError";
        break;
    case QAbstractVideoSurface::ResourceError:
        dbg << "ResourceError";
        break;
    default:
        dbg << "NoError";
        break;
    }
    return dbg;
}
#endif


QT_END_NAMESPACE

#include "moc_qabstractvideosurface.cpp"

