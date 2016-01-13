/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdeclarativecamera_p.h"
#include "qdeclarativecameraflash_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype CameraFlash
    \instantiates QDeclarativeCameraFlash
    \inqmlmodule QtMultimedia
    \brief An interface for flash related camera settings.
    \ingroup multimedia_qml
    \ingroup camera_qml

    CameraFlash is part of the \b{QtMultimedia 5.0} module.

    This type allows you to operate the camera flash
    hardware and control the flash mode used.  Not all cameras have
    flash hardware (and in some cases it is shared with the
    \l {Torch}{torch} hardware).

    It should not be constructed separately, instead the
    \c flash property of a \l Camera should be used.

    \qml
    import QtQuick 2.0
    import QtMultimedia 5.0

    Camera {
        id: camera

        exposure.exposureCompensation: -1.0
        flash.mode: Camera.FlashRedEyeReduction
    }

    \endqml
*/

/*!
    Construct a declarative camera flash object using \a parent object.
 */
QDeclarativeCameraFlash::QDeclarativeCameraFlash(QCamera *camera, QObject *parent) :
    QObject(parent)
{
    m_exposure = camera->exposure();
    connect(m_exposure, SIGNAL(flashReady(bool)), this, SIGNAL(flashReady(bool)));
}

QDeclarativeCameraFlash::~QDeclarativeCameraFlash()
{
}
/*!
    \property bool QDeclarativeCameraFlash::ready

    This property indicates whether the flash is charged.
*/
/*!
    \qmlproperty bool QtMultimedia::CameraFlash::ready

    This property indicates whether the flash is charged.
*/
bool QDeclarativeCameraFlash::isFlashReady() const
{
    return m_exposure->isFlashReady();
}
/*!
    \property QDeclarativeCameraFlash::mode

    This property holds the camera flash mode. The mode can be one of the constants in \l QCameraExposure::FlashMode.
*/
/*!
    \qmlproperty enumeration QtMultimedia::CameraFlash::mode

    This property holds the camera flash mode.

    The mode can be one of the following:
    \table
    \header \li Value \li Description
    \row \li Camera.FlashOff             \li Flash is Off.
    \row \li Camera.FlashOn              \li Flash is On.
    \row \li Camera.FlashAuto            \li Automatic flash.
    \row \li Camera.FlashRedEyeReduction \li Red eye reduction flash.
    \row \li Camera.FlashFill            \li Use flash to fillin shadows.
    \row \li Camera.FlashTorch           \li Constant light source, useful for focusing and video capture.
    \row \li Camera.FlashSlowSyncFrontCurtain
                                \li Use the flash in conjunction with a slow shutter speed.
                                This mode allows better exposure of distant objects and/or motion blur effect.
    \row \li Camera.FlashSlowSyncRearCurtain
                                \li The similar mode to FlashSlowSyncFrontCurtain but flash is fired at the end of exposure.
    \row \li Camera.FlashManual          \li Flash power is manually set.
    \endtable

*/
QDeclarativeCameraFlash::FlashMode QDeclarativeCameraFlash::flashMode() const
{
    return QDeclarativeCameraFlash::FlashMode(int(m_exposure->flashMode()));
}

void QDeclarativeCameraFlash::setFlashMode(QDeclarativeCameraFlash::FlashMode mode)
{
    if (flashMode() != mode) {
        m_exposure->setFlashMode(QCameraExposure::FlashModes(mode));
        emit flashModeChanged(mode);
    }
}

/*!
    \qmlsignal QtMultimedia::CameraFlash::flashModeChanged(int)
    This signal is emitted when the \c flashMode property is changed.
    The corresponding handler is \c onFlashModeChanged.
*/

/*!
    \qmlsignal QtMultimedia::CameraFlash::flashReady(bool)
    This signal is emitted when QCameraExposure indicates that
    the flash is ready to use.
    The corresponding handler is \c onFlashReadyChanged.
*/

QT_END_NAMESPACE

#include "moc_qdeclarativecameraflash_p.cpp"
