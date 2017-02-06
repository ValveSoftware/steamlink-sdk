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

#include "qcameraviewfindersettingscontrol.h"
#include "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QCameraViewfinderSettingsControl
    \inmodule QtMultimedia


    \ingroup multimedia_control


    \brief The QCameraViewfinderSettingsControl class provides an abstract class
    for controlling camera viewfinder parameters.

    The interface name of QCameraViewfinderSettingsControl is \c org.qt-project.qt.cameraviewfindersettingscontrol/5.0 as
    defined in QCameraViewfinderSettingsControl_iid.

    \warning New backends should implement QCameraViewfinderSettingsControl2 instead.
    Application developers should request this control only if QCameraViewfinderSettingsControl2
    is not available.

    \sa QMediaService::requestControl(), QCameraViewfinderSettingsControl2, QCamera
*/

/*!
    \macro QCameraViewfinderSettingsControl_iid

    \c org.qt-project.qt.cameraviewfindersettingscontrol/5.0

    Defines the interface name of the QCameraViewfinderSettingsControl class.

    \relates QCameraViewfinderSettingsControl
*/

/*!
    Constructs a camera viewfinder control object with \a parent.
*/
QCameraViewfinderSettingsControl::QCameraViewfinderSettingsControl(QObject *parent)
    : QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destroys the camera viewfinder control object.
*/
QCameraViewfinderSettingsControl::~QCameraViewfinderSettingsControl()
{
}

/*!
  \enum QCameraViewfinderSettingsControl::ViewfinderParameter
  \value Resolution
         Viewfinder resolution, QSize.
  \value PixelAspectRatio
         Pixel aspect ratio, QSize as in QVideoSurfaceFormat::pixelAspectRatio
  \value MinimumFrameRate
         Minimum viewfinder frame rate, qreal
  \value MaximumFrameRate
         Maximum viewfinder frame rate, qreal
  \value PixelFormat
         Viewfinder pixel format, QVideoFrame::PixelFormat
  \value UserParameter
         The base value for platform specific extended parameters.
         For such parameters the sequential values starting from UserParameter should be used.
*/

/*!
    \fn bool QCameraViewfinderSettingsControl::isViewfinderParameterSupported(ViewfinderParameter parameter) const

    Returns true if configuration of viewfinder \a parameter is supported by camera backend.
*/

/*!
    \fn QCameraViewfinderSettingsControl::viewfinderParameter(ViewfinderParameter parameter) const

    Returns the value of viewfinder \a parameter.
*/

/*!
    \fn QCameraViewfinderSettingsControl::setViewfinderParameter(ViewfinderParameter parameter, const QVariant &value)

    Set the prefferred \a value of viewfinder \a parameter.

    Calling this while the camera is active may result in the camera being
    stopped and reloaded. If video recording is in progress, this call may be ignored.

    If an unsupported parameter is specified the camera may fail to load,
    or the setting may be ignored.

    Viewfinder parameters may also depend on other camera settings,
    especially in video capture mode. If camera configuration conflicts
    with viewfinder settings, the camara configuration is usually preferred.
*/


/*!
    \class QCameraViewfinderSettingsControl2
    \inmodule QtMultimedia
    \ingroup multimedia_control
    \since 5.5

    \brief The QCameraViewfinderSettingsControl2 class provides access to the viewfinder settings
    of a camera media service.

    The functionality provided by this control is exposed to application code through the QCamera class.

    The interface name of QCameraViewfinderSettingsControl2 is \c org.qt-project.qt.cameraviewfindersettingscontrol2/5.5 as
    defined in QCameraViewfinderSettingsControl2_iid.

    \sa QMediaService::requestControl(), QCameraViewfinderSettings, QCamera
*/

/*!
    \macro QCameraViewfinderSettingsControl2_iid

    \c org.qt-project.qt.cameraviewfindersettingscontrol2/5.5

    Defines the interface name of the QCameraViewfinderSettingsControl2 class.

    \relates QCameraViewfinderSettingsControl2
*/

/*!
    Constructs a camera viewfinder settings control object with \a parent.
*/
QCameraViewfinderSettingsControl2::QCameraViewfinderSettingsControl2(QObject *parent)
    : QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destroys the camera viewfinder settings control object.
*/
QCameraViewfinderSettingsControl2::~QCameraViewfinderSettingsControl2()
{
}

/*!
    \fn QCameraViewfinderSettingsControl2::supportedViewfinderSettings() const

    Returns a list of supported camera viewfinder settings.

    The list is ordered by preference; preferred settings come first.
*/

/*!
    \fn QCameraViewfinderSettingsControl2::viewfinderSettings() const

    Returns the viewfinder settings.

    If undefined or unsupported values are passed to QCameraViewfinderSettingsControl2::setViewfinderSettings(),
    this function returns the actual settings used by the camera viewfinder. These may be available
    only once the camera is active.
*/

/*!
    \fn QCameraViewfinderSettingsControl2::setViewfinderSettings(const QCameraViewfinderSettings &settings)

    Sets the camera viewfinder \a settings.
*/

#include "moc_qcameraviewfindersettingscontrol.cpp"
QT_END_NAMESPACE

