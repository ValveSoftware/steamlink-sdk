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

#include <qcameraflashcontrol.h>
#include  "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QCameraFlashControl

    \brief The QCameraFlashControl class allows controlling a camera's flash.

    \inmodule QtMultimedia


    \ingroup multimedia_control

    You can set the type of flash effect used when an image is captured, and test to see
    if the flash hardware is ready to fire.

    You can retrieve this control from the camera object in the usual way:

    Some camera devices may not have flash hardware, or may not be configurable.  In that
    case, there will be no QCameraFlashControl available.

    The interface name of QCameraFlashControl is \c org.qt-project.qt.cameraflashcontrol/5.0 as
    defined in QCameraFlashControl_iid.

    \sa QCamera
*/

/*!
    \macro QCameraFlashControl_iid

    \c org.qt-project.qt.cameraflashcontrol/5.0

    Defines the interface name of the QCameraFlashControl class.

    \relates QCameraFlashControl
*/

/*!
    Constructs a camera flash control object with \a parent.
*/
QCameraFlashControl::QCameraFlashControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destroys the camera control object.
*/
QCameraFlashControl::~QCameraFlashControl()
{
}

/*!
  \fn QCamera::FlashModes QCameraFlashControl::flashMode() const

  Returns the current flash mode.
*/

/*!
  \fn void QCameraFlashControl::setFlashMode(QCameraExposure::FlashModes mode)

  Set the current flash \a mode.

  Usually a single QCameraExposure::FlashMode flag is used,
  but some non conflicting flags combination are also allowed,
  like QCameraExposure::FlashManual | QCameraExposure::FlashSlowSyncRearCurtain.
*/


/*!
  \fn QCameraFlashControl::isFlashModeSupported(QCameraExposure::FlashModes mode) const

  Return true if the reqested flash \a mode is supported.
  Some QCameraExposure::FlashMode values can be combined,
  for example QCameraExposure::FlashManual | QCameraExposure::FlashSlowSyncRearCurtain
*/

/*!
  \fn bool QCameraFlashControl::isFlashReady() const

  Returns true if flash is charged.
*/

/*!
    \fn void QCameraFlashControl::flashReady(bool ready)

    Signal emitted when flash state changes to \a ready.
*/

#include "moc_qcameraflashcontrol.cpp"
QT_END_NAMESPACE

