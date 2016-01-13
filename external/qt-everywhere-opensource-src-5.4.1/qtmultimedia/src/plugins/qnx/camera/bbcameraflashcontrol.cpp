/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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
#include "bbcameraflashcontrol.h"

#include "bbcamerasession.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

BbCameraFlashControl::BbCameraFlashControl(BbCameraSession *session, QObject *parent)
    : QCameraFlashControl(parent)
    , m_session(session)
    , m_flashMode(QCameraExposure::FlashAuto)
{
}

QCameraExposure::FlashModes BbCameraFlashControl::flashMode() const
{
    return m_flashMode;
}

void BbCameraFlashControl::setFlashMode(QCameraExposure::FlashModes mode)
{
    if (m_flashMode == mode)
        return;

    if (m_session->status() != QCamera::ActiveStatus) // can only be changed when viewfinder is active
        return;

    if (m_flashMode == QCameraExposure::FlashVideoLight) {
        const camera_error_t result = camera_config_videolight(m_session->handle(), CAMERA_VIDEOLIGHT_OFF);
        if (result != CAMERA_EOK)
            qWarning() << "Unable to switch off video light:" << result;
    }

    m_flashMode = mode;

    if (m_flashMode == QCameraExposure::FlashVideoLight) {
        const camera_error_t result = camera_config_videolight(m_session->handle(), CAMERA_VIDEOLIGHT_ON);
        if (result != CAMERA_EOK)
            qWarning() << "Unable to switch on video light:" << result;
    } else {
        camera_flashmode_t flashMode = CAMERA_FLASH_AUTO;

        if (m_flashMode.testFlag(QCameraExposure::FlashAuto)) flashMode = CAMERA_FLASH_AUTO;
        else if (mode.testFlag(QCameraExposure::FlashOff)) flashMode = CAMERA_FLASH_OFF;
        else if (mode.testFlag(QCameraExposure::FlashOn)) flashMode = CAMERA_FLASH_ON;

        const camera_error_t result = camera_config_flash(m_session->handle(), flashMode);
        if (result != CAMERA_EOK)
            qWarning() << "Unable to configure flash:" << result;
    }
}

bool BbCameraFlashControl::isFlashModeSupported(QCameraExposure::FlashModes mode) const
{
    bool supportsVideoLight = false;
    if (m_session->handle() != CAMERA_HANDLE_INVALID) {
        supportsVideoLight = camera_has_feature(m_session->handle(), CAMERA_FEATURE_VIDEOLIGHT);
    }

    return  (mode == QCameraExposure::FlashOff ||
             mode == QCameraExposure::FlashOn ||
             mode == QCameraExposure::FlashAuto ||
             ((mode == QCameraExposure::FlashVideoLight) && supportsVideoLight));
}

bool BbCameraFlashControl::isFlashReady() const
{
    //TODO: check for flash charge-level here?!?
    return true;
}

QT_END_NAMESPACE
