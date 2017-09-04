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

#include "avfcamerafocuscontrol.h"
#include "avfcamerautility.h"
#include "avfcameraservice.h"
#include "avfcamerasession.h"
#include "avfcameradebug.h"

#include <QtCore/qdebug.h>

#include <AVFoundation/AVFoundation.h>

QT_BEGIN_NAMESPACE

namespace {

bool qt_focus_mode_supported(QCameraFocus::FocusModes mode)
{
    // Check if QCameraFocus::FocusMode has counterpart in AVFoundation.

    // AVFoundation has 'Manual', 'Auto' and 'Continuous',
    // where 'Manual' is actually 'Locked' + writable property 'lensPosition'.
    // Since Qt does not provide an API to manipulate a lens position, 'Maual' mode
    // (at the moment) is not supported.
    return mode == QCameraFocus::AutoFocus
           || mode == QCameraFocus::ContinuousFocus;
}

bool qt_focus_point_mode_supported(QCameraFocus::FocusPointMode mode)
{
    return mode == QCameraFocus::FocusPointAuto
           || mode == QCameraFocus::FocusPointCustom
           || mode == QCameraFocus::FocusPointCenter;
}

AVCaptureFocusMode avf_focus_mode(QCameraFocus::FocusModes requestedMode)
{
    if (requestedMode == QCameraFocus::AutoFocus)
        return AVCaptureFocusModeAutoFocus;

    return AVCaptureFocusModeContinuousAutoFocus;
}

}

AVFCameraFocusControl::AVFCameraFocusControl(AVFCameraService *service)
    : m_session(service->session()),
      m_focusMode(QCameraFocus::ContinuousFocus),
      m_focusPointMode(QCameraFocus::FocusPointAuto),
      m_customFocusPoint(0.5f, 0.5f),
      m_actualFocusPoint(m_customFocusPoint)
{
    Q_ASSERT(m_session);
    connect(m_session, SIGNAL(stateChanged(QCamera::State)), SLOT(cameraStateChanged()));
}

QCameraFocus::FocusModes AVFCameraFocusControl::focusMode() const
{
    return m_focusMode;
}

void AVFCameraFocusControl::setFocusMode(QCameraFocus::FocusModes mode)
{
    if (m_focusMode == mode)
        return;

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice) {
        if (qt_focus_mode_supported(mode)) {
            m_focusMode = mode;
            Q_EMIT focusModeChanged(m_focusMode);
        } else {
            qDebugCamera() << Q_FUNC_INFO
                           << "focus mode not supported";
        }
        return;
    }

    if (isFocusModeSupported(mode)) {
        const AVFConfigurationLock lock(captureDevice);
        if (!lock) {
            qDebugCamera() << Q_FUNC_INFO
                           << "failed to lock for configuration";
            return;
        }

        captureDevice.focusMode = avf_focus_mode(mode);
        m_focusMode = mode;
    } else {
        qDebugCamera() << Q_FUNC_INFO << "focus mode not supported";
        return;
    }

    Q_EMIT focusModeChanged(m_focusMode);
}

bool AVFCameraFocusControl::isFocusModeSupported(QCameraFocus::FocusModes mode) const
{
    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice)
        return false;

    if (!qt_focus_mode_supported(mode))
        return false;

    return [captureDevice isFocusModeSupported:avf_focus_mode(mode)];
}

QCameraFocus::FocusPointMode AVFCameraFocusControl::focusPointMode() const
{
    return m_focusPointMode;
}

void AVFCameraFocusControl::setFocusPointMode(QCameraFocus::FocusPointMode mode)
{
    if (m_focusPointMode == mode)
        return;

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice) {
        if (qt_focus_point_mode_supported(mode)) {
            m_focusPointMode = mode;
            Q_EMIT focusPointModeChanged(mode);
        }
        return;
    }

    if (isFocusPointModeSupported(mode)) {
        const AVFConfigurationLock lock(captureDevice);
        if (!lock) {
            qDebugCamera() << Q_FUNC_INFO << "failed to lock for configuration";
            return;
        }

        bool resetPOI = false;
        if (mode == QCameraFocus::FocusPointCenter || mode == QCameraFocus::FocusPointAuto) {
            if (m_actualFocusPoint != QPointF(0.5, 0.5)) {
                m_actualFocusPoint = QPointF(0.5, 0.5);
                resetPOI = true;
            }
        } else if (mode == QCameraFocus::FocusPointCustom) {
            if (m_actualFocusPoint != m_customFocusPoint) {
                m_actualFocusPoint = m_customFocusPoint;
                resetPOI = true;
            }
        } // else for any other mode in future.

        if (resetPOI) {
            const CGPoint focusPOI = CGPointMake(m_actualFocusPoint.x(), m_actualFocusPoint.y());
            [captureDevice setFocusPointOfInterest:focusPOI];
        }
        m_focusPointMode = mode;
    } else {
        qDebugCamera() << Q_FUNC_INFO << "focus point mode is not supported";
        return;
    }

    Q_EMIT focusPointModeChanged(mode);
}

bool AVFCameraFocusControl::isFocusPointModeSupported(QCameraFocus::FocusPointMode mode) const
{
    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice)
        return false;

    if (!qt_focus_point_mode_supported(mode))
        return false;

    return [captureDevice isFocusPointOfInterestSupported];
}

QPointF AVFCameraFocusControl::customFocusPoint() const
{
    return m_customFocusPoint;
}

void AVFCameraFocusControl::setCustomFocusPoint(const QPointF &point)
{
    if (m_customFocusPoint == point)
        return;

    if (!QRectF(0.f, 0.f, 1.f, 1.f).contains(point)) {
        qDebugCamera() << Q_FUNC_INFO << "invalid focus point (out of range)";
        return;
    }

    m_customFocusPoint = point;
    Q_EMIT customFocusPointChanged(m_customFocusPoint);

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice || m_focusPointMode != QCameraFocus::FocusPointCustom)
        return;

    if ([captureDevice isFocusPointOfInterestSupported]) {
        const AVFConfigurationLock lock(captureDevice);
        if (!lock) {
            qDebugCamera() << Q_FUNC_INFO << "failed to lock for configuration";
            return;
        }

        m_actualFocusPoint = m_customFocusPoint;
        const CGPoint focusPOI = CGPointMake(point.x(), point.y());
        [captureDevice setFocusPointOfInterest:focusPOI];
        if (m_focusMode != QCameraFocus::ContinuousFocus)
            [captureDevice setFocusMode:AVCaptureFocusModeAutoFocus];
    } else {
        qDebugCamera() << Q_FUNC_INFO << "focus point of interest not supported";
        return;
    }
}

QCameraFocusZoneList AVFCameraFocusControl::focusZones() const
{
    // Unsupported.
    return QCameraFocusZoneList();
}

void AVFCameraFocusControl::cameraStateChanged()
{
    if (m_session->state() != QCamera::ActiveState)
        return;

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice) {
        qDebugCamera() << Q_FUNC_INFO << "capture device is nil in 'active' state";
        return;
    }

    const AVFConfigurationLock lock(captureDevice);
    if (m_customFocusPoint != m_actualFocusPoint
        && m_focusPointMode == QCameraFocus::FocusPointCustom) {
        if (![captureDevice isFocusPointOfInterestSupported]) {
            qDebugCamera() << Q_FUNC_INFO
                           << "focus point of interest not supported";
            return;
        }

        if (!lock) {
            qDebugCamera() << Q_FUNC_INFO << "failed to lock for configuration";
            return;
        }

        m_actualFocusPoint = m_customFocusPoint;
        const CGPoint focusPOI = CGPointMake(m_customFocusPoint.x(), m_customFocusPoint.y());
        [captureDevice setFocusPointOfInterest:focusPOI];
    }

    if (m_focusMode != QCameraFocus::ContinuousFocus) {
        const AVCaptureFocusMode avMode = avf_focus_mode(m_focusMode);
        if (captureDevice.focusMode != avMode) {
            if (![captureDevice isFocusModeSupported:avMode]) {
                qDebugCamera() << Q_FUNC_INFO << "focus mode not supported";
                return;
            }

            if (!lock) {
                qDebugCamera() << Q_FUNC_INFO << "failed to lock for configuration";
                return;
            }

            [captureDevice setFocusMode:avMode];
        }
    }
}

QT_END_NAMESPACE

#include "moc_avfcamerafocuscontrol.cpp"
