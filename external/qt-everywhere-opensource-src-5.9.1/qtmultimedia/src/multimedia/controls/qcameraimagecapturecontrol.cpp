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

#include <qcameraimagecapturecontrol.h>
#include <QtCore/qstringlist.h>

QT_BEGIN_NAMESPACE

/*!
    \class QCameraImageCaptureControl

    \brief The QCameraImageCaptureControl class provides a control interface
    for image capture services.

    \inmodule QtMultimedia


    \ingroup multimedia_control

    The interface name of QCameraImageCaptureControl is \c org.qt-project.qt.cameraimagecapturecontrol/5.0 as
    defined in QCameraImageCaptureControl_iid.


    \sa QMediaService::requestControl()
*/

/*!
    \macro QCameraImageCaptureControl_iid

    \c org.qt-project.qt.cameraimagecapturecontrol/5.0

    Defines the interface name of the QCameraImageCaptureControl class.

    \relates QCameraImageCaptureControl
*/

/*!
    Constructs a new image capture control object with the given \a parent
*/
QCameraImageCaptureControl::QCameraImageCaptureControl(QObject *parent)
    :QMediaControl(parent)
{
}

/*!
    Destroys an image capture control.
*/
QCameraImageCaptureControl::~QCameraImageCaptureControl()
{
}

/*!
    \fn QCameraImageCaptureControl::isReadyForCapture() const

    Identifies if a capture control is ready to perform a capture
    immediately (all the resources necessary for image capture are allocated,
    hardware initialized, flash is charged, etc).

    Returns true if the camera is ready for capture; and false if it is not.

    It's permissible to call capture() while the camera status is QCamera::ActiveStatus
    regardless of isReadyForCapture property value.
    If camera is not ready to capture image immediately,
    the capture request is queued with all the related camera settings
    to be executed as soon as possible.
*/

/*!
    \fn QCameraImageCaptureControl::readyForCaptureChanged(bool ready)

    Signals that a capture control's \a ready state has changed.
*/

/*!
    \fn QCameraImageCaptureControl::capture(const QString &fileName)

    Initiates the capture of an image to \a fileName.
    The \a fileName can be relative or empty,
    in this case the service should use the system specific place
    and file naming scheme.

    The Camera service should save all the capture parameters
    like exposure settings or image processing parameters,
    so changes to camera parameters after capture() is called
    do not affect previous capture requests.

    Returns the capture request id number, which is used later
    with imageExposed(), imageCaptured() and imageSaved() signals.
*/

/*!
    \fn QCameraImageCaptureControl::cancelCapture()

    Cancel pending capture requests.
*/

/*!
    \fn QCameraImageCaptureControl::imageExposed(int requestId)

    Signals that an image with it \a requestId
    has just been exposed.
    This signal can be used for the shutter sound or other indicaton.
*/

/*!
    \fn QCameraImageCaptureControl::imageCaptured(int requestId, const QImage &preview)

    Signals that an image with it \a requestId
    has been captured and a \a preview is available.
*/

/*!
    \fn QCameraImageCaptureControl::imageMetadataAvailable(int id, const QString &key, const QVariant &value)

    Signals that a metadata for an image with request \a id is available. Signal
    also contains the \a key and \a value of the metadata.

    This signal should be emitted between imageExposed and imageSaved signals.
*/

/*!
    \fn QCameraImageCaptureControl::imageAvailable(int requestId, const QVideoFrame &buffer)

    Signals that a captured \a buffer with a \a requestId is available.
*/

/*!
    \fn QCameraImageCaptureControl::imageSaved(int requestId, const QString &fileName)

    Signals that a captured image with a \a requestId has been saved
    to \a fileName.
*/

/*!
    \fn QCameraImageCaptureControl::driveMode() const

    Returns the current camera drive mode.
*/

/*!
    \fn QCameraImageCaptureControl::setDriveMode(QCameraImageCapture::DriveMode mode)

    Sets the current camera drive \a mode.
*/


/*!
    \fn QCameraImageCaptureControl::error(int id, int error, const QString &errorString)

    Signals the capture request \a id failed with \a error code and message \a errorString.

    \sa QCameraImageCapture::Error
*/


#include "moc_qcameraimagecapturecontrol.cpp"
QT_END_NAMESPACE

