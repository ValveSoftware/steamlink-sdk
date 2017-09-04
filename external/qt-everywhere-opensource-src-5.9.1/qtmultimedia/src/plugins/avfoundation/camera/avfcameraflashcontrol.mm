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

#include "avfcameraflashcontrol.h"
#include "avfcamerautility.h"
#include "avfcamerasession.h"
#include "avfcameraservice.h"
#include "avfcameradebug.h"

#include <QtCore/qdebug.h>

#include <AVFoundation/AVFoundation.h>


AVFCameraFlashControl::AVFCameraFlashControl(AVFCameraService *service)
    : m_service(service)
    , m_session(0)
    , m_supportedModes(QCameraExposure::FlashOff)
    , m_flashMode(QCameraExposure::FlashOff)
{
    Q_ASSERT(service);
    m_session = m_service->session();
    Q_ASSERT(m_session);

    connect(m_session, SIGNAL(stateChanged(QCamera::State)), SLOT(cameraStateChanged(QCamera::State)));
}

QCameraExposure::FlashModes AVFCameraFlashControl::flashMode() const
{
    return m_flashMode;
}

void AVFCameraFlashControl::setFlashMode(QCameraExposure::FlashModes mode)
{
    if (m_flashMode == mode)
        return;

    if (m_session->state() == QCamera::ActiveState && !isFlashModeSupported(mode)) {
        qDebugCamera() << Q_FUNC_INFO << "unsupported mode" << mode;
        return;
    }

    m_flashMode = mode;

    if (m_session->state() != QCamera::ActiveState)
        return;

    applyFlashSettings();
}

bool AVFCameraFlashControl::isFlashModeSupported(QCameraExposure::FlashModes mode) const
{
    // From what QCameraExposure has, we can support only these:
    //  FlashAuto = 0x1,
    //  FlashOff = 0x2,
    //  FlashOn = 0x4,
    // AVCaptureDevice has these flash modes:
    //  AVCaptureFlashModeAuto
    //  AVCaptureFlashModeOff
    //  AVCaptureFlashModeOn
    // QCameraExposure also has:
    //  FlashTorch = 0x20,       --> "Constant light source."
    //  FlashVideoLight = 0x40.  --> "Constant light source."
    // AVCaptureDevice:
    //  AVCaptureTorchModeOff (no mapping)
    //  AVCaptureTorchModeOn     --> FlashVideoLight
    //  AVCaptureTorchModeAuto (no mapping)

    return m_supportedModes & mode;
}

bool AVFCameraFlashControl::isFlashReady() const
{
    if (m_session->state() != QCamera::ActiveState)
        return false;

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice)
        return false;

    if (!captureDevice.hasFlash && !captureDevice.hasTorch)
        return false;

    if (!isFlashModeSupported(m_flashMode))
        return false;

#ifdef Q_OS_IOS
    // AVCaptureDevice's docs:
    // "The flash may become unavailable if, for example,
    //  the device overheats and needs to cool off."
    if (m_flashMode != QCameraExposure::FlashVideoLight)
        return [captureDevice isFlashAvailable];

    return [captureDevice isTorchAvailable];
#endif

    return true;
}

void AVFCameraFlashControl::cameraStateChanged(QCamera::State newState)
{
    if (newState == QCamera::UnloadedState) {
        m_supportedModes = QCameraExposure::FlashOff;
        Q_EMIT flashReady(false);
    } else if (newState == QCamera::ActiveState) {
        m_supportedModes = QCameraExposure::FlashOff;
        AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
        if (!captureDevice) {
            qDebugCamera() << Q_FUNC_INFO << "no capture device in 'Active' state";
            Q_EMIT flashReady(false);
            return;
        }

        if (captureDevice.hasFlash) {
            if ([captureDevice isFlashModeSupported:AVCaptureFlashModeOn])
                m_supportedModes |= QCameraExposure::FlashOn;
            if ([captureDevice isFlashModeSupported:AVCaptureFlashModeAuto])
                m_supportedModes |= QCameraExposure::FlashAuto;
        }

        if (captureDevice.hasTorch && [captureDevice isTorchModeSupported:AVCaptureTorchModeOn])
            m_supportedModes |= QCameraExposure::FlashVideoLight;

        Q_EMIT flashReady(applyFlashSettings());
    }
}

bool AVFCameraFlashControl::applyFlashSettings()
{
    Q_ASSERT(m_session->requestedState() == QCamera::ActiveState);

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice) {
        qDebugCamera() << Q_FUNC_INFO << "no capture device found";
        return false;
    }

    if (!isFlashModeSupported(m_flashMode)) {
        qDebugCamera() << Q_FUNC_INFO << "unsupported mode" << m_flashMode;
        return false;
    }

    if (!captureDevice.hasFlash && !captureDevice.hasTorch) {
        // FlashOff is the only mode we support.
        // Return false - flash is not ready.
        return false;
    }

    const AVFConfigurationLock lock(captureDevice);

    if (m_flashMode != QCameraExposure::FlashVideoLight) {
        if (captureDevice.torchMode != AVCaptureTorchModeOff) {
#ifdef Q_OS_IOS
            if (![captureDevice isTorchAvailable]) {
                qDebugCamera() << Q_FUNC_INFO << "torch is not available at the moment";
                return false;
            }
#endif
            captureDevice.torchMode = AVCaptureTorchModeOff;
        }
#ifdef Q_OS_IOS
        if (![captureDevice isFlashAvailable]) {
            // We'd like to switch flash (into some mode), but it's not available:
            qDebugCamera() << Q_FUNC_INFO << "flash is not available at the moment";
            return false;
        }
#endif
    } else {
        if (captureDevice.flashMode != AVCaptureFlashModeOff) {
#ifdef Q_OS_IOS
            if (![captureDevice isFlashAvailable]) {
                qDebugCamera() << Q_FUNC_INFO << "flash is not available at the moment";
                return false;
            }
#endif
            captureDevice.flashMode = AVCaptureFlashModeOff;
        }

#ifdef Q_OS_IOS
        if (![captureDevice isTorchAvailable]) {
            qDebugCamera() << Q_FUNC_INFO << "torch is not available at the moment";
            return false;
        }
#endif
    }

    if (m_flashMode == QCameraExposure::FlashOff)
        captureDevice.flashMode = AVCaptureFlashModeOff;
    else if (m_flashMode == QCameraExposure::FlashOn)
        captureDevice.flashMode = AVCaptureFlashModeOn;
    else if (m_flashMode == QCameraExposure::FlashAuto)
        captureDevice.flashMode = AVCaptureFlashModeAuto;
    else if (m_flashMode == QCameraExposure::FlashVideoLight)
        captureDevice.torchMode = AVCaptureTorchModeOn;

    return true;
}
