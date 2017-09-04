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

#include <qcameracontrol.h>
#include  "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QCameraControl



    \brief The QCameraControl class is an abstract base class for
    classes that control still cameras or video cameras.

    \inmodule QtMultimedia

    \ingroup multimedia_control

    This service is provided by a QMediaService object via
    QMediaService::control().  It is used by QCamera.

    The interface name of QCameraControl is \c org.qt-project.qt.cameracontrol/5.0 as
    defined in QCameraControl_iid.



    \sa QMediaService::requestControl(), QCamera
*/

/*!
    \macro QCameraControl_iid

    \c org.qt-project.qt.cameracontrol/5.0

    Defines the interface name of the QCameraControl class.

    \relates QCameraControl
*/

/*!
    Constructs a camera control object with \a parent.
*/

QCameraControl::QCameraControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destruct the camera control object.
*/

QCameraControl::~QCameraControl()
{
}

/*!
    \fn QCameraControl::state() const

    Returns the state of the camera service.

    \sa QCamera::state
*/

/*!
    \fn QCameraControl::setState(QCamera::State state)

    Sets the camera \a state.

    State changes are synchronous and indicate user intention,
    while camera status is used as a feedback mechanism to inform application about backend status.
    Status changes are reported asynchronously with QCameraControl::statusChanged() signal.

    \sa QCamera::State
*/

/*!
    \fn void QCameraControl::stateChanged(QCamera::State state)

    Signal emitted when the camera \a state changes.

    In most cases the state chage is caused by QCameraControl::setState(),
    but if critical error has occurred the state changes to QCamera::UnloadedState.
*/

/*!
    \fn QCameraControl::status() const

    Returns the status of the camera service.

    \sa QCamera::state
*/

/*!
    \fn void QCameraControl::statusChanged(QCamera::Status status)

    Signal emitted when the camera \a status changes.
*/


/*!
    \fn void QCameraControl::error(int error, const QString &errorString)

    Signal emitted when an error occurs with error code \a error and
    a description of the error \a errorString.
*/

/*!
    \fn Camera::CaptureModes QCameraControl::captureMode() const = 0

    Returns the current capture mode.
*/

/*!
    \fn void QCameraControl::setCaptureMode(QCamera::CaptureModes mode) = 0;

    Sets the current capture \a mode.

    The capture mode changes are synchronous and allowed in any camera state.

    If the capture mode is changed while camera is active,
    it's recommended to change status to QCamera::LoadedStatus
    and start activating the camera in the next event loop
    with the status changed to QCamera::StartingStatus.
    This allows the capture settings to be applied before camera is started.
    Than change the status to QCamera::StartedStatus when the capture mode change is done.
*/

/*!
    \fn bool QCameraControl::isCaptureModeSupported(QCamera::CaptureModes mode) const = 0;

    Returns true if the capture \a mode is suported.
*/

/*!
    \fn QCameraControl::captureModeChanged(QCamera::CaptureModes mode)

    Signal emitted when the camera capture \a mode changes.
 */

/*!
    \fn bool QCameraControl::canChangeProperty(PropertyChangeType changeType, QCamera::Status status) const

    Returns true if backend can effectively apply changing camera properties of \a changeType type
    while the camera state is QCamera::Active and camera status matches \a status parameter.

    If backend doesn't support applying this change in the active state, it will be stopped
    before the settings are changed and restarted after.
    Otherwise the backend should apply the change in the current state,
    with the camera status indicating the progress, if necessary.
*/

/*!
  \enum QCameraControl::PropertyChangeType

  \value CaptureMode Indicates the capture mode is changed.
  \value ImageEncodingSettings Image encoder settings are changed, including resolution.
  \value VideoEncodingSettings
        Video encoder settings are changed, including audio, video and container settings.
  \value Viewfinder Viewfinder is changed.
  \value ViewfinderSettings Viewfinder settings are changed.
*/

#include "moc_qcameracontrol.cpp"
QT_END_NAMESPACE
