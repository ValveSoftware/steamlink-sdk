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

#include "qcameraviewfindersettings.h"

QT_BEGIN_NAMESPACE

static void qRegisterViewfinderSettingsMetaType()
{
    qRegisterMetaType<QCameraViewfinderSettings>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterViewfinderSettingsMetaType)


class QCameraViewfinderSettingsPrivate  : public QSharedData
{
public:
    QCameraViewfinderSettingsPrivate() :
        isNull(true),
        minimumFrameRate(0.0),
        maximumFrameRate(0.0),
        pixelFormat(QVideoFrame::Format_Invalid)
    {
    }

    QCameraViewfinderSettingsPrivate(const QCameraViewfinderSettingsPrivate &other):
        QSharedData(other),
        isNull(other.isNull),
        resolution(other.resolution),
        minimumFrameRate(other.minimumFrameRate),
        maximumFrameRate(other.maximumFrameRate),
        pixelFormat(other.pixelFormat),
        pixelAspectRatio(other.pixelAspectRatio)
    {
    }

    bool isNull;
    QSize resolution;
    qreal minimumFrameRate;
    qreal maximumFrameRate;
    QVideoFrame::PixelFormat pixelFormat;
    QSize pixelAspectRatio;

private:
    QCameraViewfinderSettingsPrivate& operator=(const QCameraViewfinderSettingsPrivate &other);
};


/*!
    \class QCameraViewfinderSettings
    \since 5.5
    \brief The QCameraViewfinderSettings class provides a set of viewfinder settings.

    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_camera

    A viewfinder settings object is used to specify the viewfinder settings used by QCamera.
    Viewfinder settings are selected by constructing a QCameraViewfinderSettings object,
    setting the desired properties and then passing it to a QCamera instance using the
    QCamera::setViewfinderSettings() function.

    \snippet multimedia-snippets/camera.cpp Camera viewfinder settings

    Different cameras may have different capabilities. The application should query the camera
    capabilities before setting parameters. For example, the application should call
    QCamera::supportedViewfinderResolutions() before calling setResolution().

    \sa QCamera
*/

/*!
    Constructs a null viewfinder settings object.
*/
QCameraViewfinderSettings::QCameraViewfinderSettings()
    : d(new QCameraViewfinderSettingsPrivate)
{
}

/*!
    Constructs a copy of the viewfinder settings object \a other.
*/
QCameraViewfinderSettings::QCameraViewfinderSettings(const QCameraViewfinderSettings &other)
    : d(other.d)
{

}

/*!
    Destroys a viewfinder settings object.
*/
QCameraViewfinderSettings::~QCameraViewfinderSettings()
{

}

/*!
    Assigns the value of \a other to a viewfinder settings object.
*/
QCameraViewfinderSettings &QCameraViewfinderSettings::operator=(const QCameraViewfinderSettings &other)
{
    d = other.d;
    return *this;
}

/*! \fn QCameraViewfinderSettings &QCameraViewfinderSettings::operator=(QCameraViewfinderSettings &&other)

    Moves \a other to this viewfinder settings object and returns a reference to this object.
*/

/*!
    \fn void QCameraViewfinderSettings::swap(QCameraViewfinderSettings &other)

    Swaps this viewfinder settings object with \a other. This
    function is very fast and never fails.
*/

/*!
    \relates QCameraViewfinderSettings
    \since 5.5

    Determines if \a lhs is of equal value to \a rhs.

    Returns true if the settings objects are of equal value, and false if they
    are not of equal value.
*/
bool operator==(const QCameraViewfinderSettings &lhs, const QCameraViewfinderSettings &rhs) Q_DECL_NOTHROW
{
    return (lhs.d == rhs.d) ||
           (lhs.d->isNull == rhs.d->isNull &&
            lhs.d->resolution == rhs.d->resolution &&
            lhs.d->minimumFrameRate == rhs.d->minimumFrameRate &&
            lhs.d->maximumFrameRate == rhs.d->maximumFrameRate &&
            lhs.d->pixelFormat == rhs.d->pixelFormat &&
            lhs.d->pixelAspectRatio == rhs.d->pixelAspectRatio);
}

/*!
    \fn bool operator!=(const QCameraViewfinderSettings &lhs, const QCameraViewfinderSettings &rhs)
    \relates QCameraViewfinderSettings
    \since 5.5

    Determines if \a lhs is of equal value to \a rhs.

    Returns true if the settings objects are not of equal value, and false if
    they are of equal value.
*/

/*!
    Identifies if a viewfinder settings object is uninitalized.

    Returns true if the settings are null, and false if they are not.
*/
bool QCameraViewfinderSettings::isNull() const
{
    return d->isNull;
}

/*!
    Returns the viewfinder resolution.
*/
QSize QCameraViewfinderSettings::resolution() const
{
    return d->resolution;
}

/*!
    Sets the viewfinder \a resolution.

    If the given \a resolution is empty, the backend makes an optimal choice based on the
    supported resolutions and the other viewfinder settings.

    If the camera is used to capture videos or images, the viewfinder resolution might be
    ignored if it conflicts with the capture resolution.

    \sa QVideoEncoderSettings::setResolution(), QImageEncoderSettings::setResolution(),
    QCamera::supportedViewfinderResolutions()
*/
void QCameraViewfinderSettings::setResolution(const QSize &resolution)
{
    d->isNull = false;
    d->resolution = resolution;
}

/*!
    \fn QCameraViewfinderSettings::setResolution(int width, int height)

    This is an overloaded function.

    Sets the \a width and \a height of the viewfinder resolution.
*/

/*!
    Returns the viewfinder minimum frame rate in frames per second.

    \sa maximumFrameRate()
*/
qreal QCameraViewfinderSettings::minimumFrameRate() const
{
    return d->minimumFrameRate;
}

/*!
    Sets the viewfinder minimum frame \a rate in frames per second.

    If the minimum frame \a rate is equal to the maximum frame rate, the frame rate is fixed.
    If not, the actual frame rate fluctuates between the minimum and the maximum.

    If the given \a rate equals to \c 0, the backend makes an optimal choice based on the
    supported frame rates and the other viewfinder settings.

    \sa setMaximumFrameRate(), QCamera::supportedViewfinderFrameRateRanges()
*/
void QCameraViewfinderSettings::setMinimumFrameRate(qreal rate)
{
    d->isNull = false;
    d->minimumFrameRate = rate;
}

/*!
    Returns the viewfinder maximum frame rate in frames per second.

    \sa minimumFrameRate()
*/
qreal QCameraViewfinderSettings::maximumFrameRate() const
{
    return d->maximumFrameRate;
}

/*!
    Sets the viewfinder maximum frame \a rate in frames per second.

    If the maximum frame \a rate is equal to the minimum frame rate, the frame rate is fixed.
    If not, the actual frame rate fluctuates between the minimum and the maximum.

    If the given \a rate equals to \c 0, the backend makes an optimal choice based on the
    supported frame rates and the other viewfinder settings.

    \sa setMinimumFrameRate(), QCamera::supportedViewfinderFrameRateRanges()
*/
void QCameraViewfinderSettings::setMaximumFrameRate(qreal rate)
{
    d->isNull = false;
    d->maximumFrameRate = rate;
}

/*!
    Returns the viewfinder pixel format.
*/
QVideoFrame::PixelFormat QCameraViewfinderSettings::pixelFormat() const
{
    return d->pixelFormat;
}

/*!
    Sets the viewfinder pixel \a format.

    If the given \a format is equal to QVideoFrame::Format_Invalid, the backend uses the
    default format.

    \sa QCamera::supportedViewfinderPixelFormats()
*/
void QCameraViewfinderSettings::setPixelFormat(QVideoFrame::PixelFormat format)
{
    d->isNull = false;
    d->pixelFormat = format;
}

/*!
    Returns the viewfinder pixel aspect ratio.
*/
QSize QCameraViewfinderSettings::pixelAspectRatio() const
{
    return d->pixelAspectRatio;
}

/*!
    Sets the viewfinder pixel aspect \a ratio.
*/
void QCameraViewfinderSettings::setPixelAspectRatio(const QSize &ratio)
{
    d->isNull = false;
    d->pixelAspectRatio = ratio;
}

/*!
    \fn QCameraViewfinderSettings::setPixelAspectRatio(int horizontal, int vertical)

    This is an overloaded function.

    Sets the \a horizontal and \a vertical elements of the viewfinder's pixel aspect ratio.
*/

QT_END_NAMESPACE
