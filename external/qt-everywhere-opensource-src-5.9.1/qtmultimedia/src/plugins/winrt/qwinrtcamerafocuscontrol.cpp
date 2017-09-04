/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "qwinrtcamerafocuscontrol.h"
#include "qwinrtcameraimagecapturecontrol.h"
#include "qwinrtcameracontrol.h"

QT_BEGIN_NAMESPACE

class QWinRTCameraFocusControlPrivate
{
public:
    QCameraFocus::FocusModes focusModes;
    QCameraFocus::FocusModes supportedFocusModes;
    QCameraFocus::FocusPointMode focusPointMode;
    QSet<QCameraFocus::FocusPointMode> supportedFocusPointModes;
    QPointF focusPoint;
    bool focusModeInitialized;
    bool focusPointModeInitialized;
    bool imageCaptureIdle;
};

QWinRTCameraFocusControl::QWinRTCameraFocusControl(QWinRTCameraControl *parent)
    : QCameraFocusControl(parent), d_ptr(new QWinRTCameraFocusControlPrivate)
{
    Q_D(QWinRTCameraFocusControl);
    d->focusModeInitialized = false;
    d->focusPointModeInitialized = false;
    d->focusModes = QCameraFocus::ContinuousFocus;
    d->focusPointMode = QCameraFocus::FocusPointAuto;
    d->imageCaptureIdle = true;
    QWinRTCameraImageCaptureControl *imageCaptureControl = static_cast<QWinRTCameraImageCaptureControl *>(parent->imageCaptureControl());
    Q_ASSERT(imageCaptureControl);
    connect(imageCaptureControl, &QWinRTCameraImageCaptureControl::captureQueueChanged,
            this, &QWinRTCameraFocusControl::imageCaptureQueueChanged, Qt::QueuedConnection);
}

QCameraFocus::FocusModes QWinRTCameraFocusControl::focusMode() const
{
    Q_D(const QWinRTCameraFocusControl);
    return d->focusModes;
}

void QWinRTCameraFocusControl::setFocusMode(QCameraFocus::FocusModes modes)
{
    Q_D(QWinRTCameraFocusControl);
    if (d->focusModes == modes)
        return;
    QWinRTCameraControl *cameraControl = static_cast<QWinRTCameraControl *>(parent());
    Q_ASSERT(cameraControl);
    if (!modes) {
        cameraControl->emitError(QCamera::InvalidRequestError, QStringLiteral("Can't set empty camera focus modes."));
        return;
    }
    if (!d->focusModeInitialized) {
        d->focusModes = modes;
        emit focusModeChanged(modes);
        return;
    }
    if (!isFocusModeSupported(modes)) {
        cameraControl->emitError(QCamera::NotSupportedFeatureError, QStringLiteral("Unsupported camera focus modes."));
        return;
    }
    if (modes.testFlag(QCameraFocus::ContinuousFocus)) {
        if (QCameraFocus::FocusPointCustom == d->focusPointMode) {
            cameraControl->emitError(QCamera::NotSupportedFeatureError,
                                     QStringLiteral("Unsupported camera focus modes: ContinuousFocus with FocusPointCustom."));
            return;
        } else if (!d->imageCaptureIdle) {
            cameraControl->emitError(QCamera::NotSupportedFeatureError,
                                     QStringLiteral("Can't set ContinuousFocus camera focus mode while capturing image."));
            return;
        }
    }
    if (!cameraControl->setFocus(modes))
        return;
    if (modes.testFlag(QCameraFocus::ContinuousFocus) || d->focusModes.testFlag(QCameraFocus::ContinuousFocus))
        cameraControl->focus();
    d->focusModes = modes;
    emit focusModeChanged(modes);
}

bool QWinRTCameraFocusControl::isFocusModeSupported(QCameraFocus::FocusModes modes) const
{
    Q_D(const QWinRTCameraFocusControl);
    return (d->focusModeInitialized && modes) ? !((d->supportedFocusModes & modes) ^ modes) : false;
}

QCameraFocus::FocusPointMode QWinRTCameraFocusControl::focusPointMode() const
{
    Q_D(const QWinRTCameraFocusControl);
    return d->focusPointMode;
}

void QWinRTCameraFocusControl::setFocusPointMode(QCameraFocus::FocusPointMode mode)
{
    Q_D(QWinRTCameraFocusControl);
    if (d->focusPointMode == mode)
        return;

    if (!d->focusModeInitialized) {
        d->focusPointMode = mode;
        emit focusPointModeChanged(mode);
        return;
    }
    QWinRTCameraControl *cameraControl = static_cast<QWinRTCameraControl *>(parent());
    Q_ASSERT(cameraControl);
    if (!d->supportedFocusPointModes.contains(mode)) {
        cameraControl->emitError(QCamera::NotSupportedFeatureError, QStringLiteral("Unsupported camera point focus mode."));
        return;
    }
    if (QCameraFocus::FocusPointCenter == mode || QCameraFocus::FocusPointAuto == mode)
        d->focusPoint = QPointF(0.5, 0.5);
    // Don't apply focus point focus settings if camera is in continuous focus mode
    if (!d->focusModes.testFlag(QCameraFocus::ContinuousFocus)) {
        changeFocusCustomPoint(d->focusPoint);
    } else if (QCameraFocus::FocusPointCustom == mode) {
        cameraControl->emitError(QCamera::NotSupportedFeatureError, QStringLiteral("Unsupported camera focus modes: ContinuousFocus with FocusPointCustom."));
        return;
    }
    d->focusPointMode = mode;
    emit focusPointModeChanged(mode);
}

bool QWinRTCameraFocusControl::isFocusPointModeSupported(QCameraFocus::FocusPointMode mode) const
{
    Q_D(const QWinRTCameraFocusControl);
    return d->supportedFocusPointModes.contains(mode);
}

QPointF QWinRTCameraFocusControl::customFocusPoint() const
{
    Q_D(const QWinRTCameraFocusControl);
    return d->focusPoint;
}

void QWinRTCameraFocusControl::setCustomFocusPoint(const QPointF &point)
{
    Q_D(QWinRTCameraFocusControl);
    if (d->focusPointMode != QCameraFocus::FocusPointCustom) {
        QWinRTCameraControl *cameraControl = static_cast<QWinRTCameraControl *>(parent());
        Q_ASSERT(cameraControl);
        cameraControl->emitError(QCamera::InvalidRequestError, QStringLiteral("Custom focus point can be set only in FocusPointCustom focus mode."));
        return;
    }
    if (d->focusPoint == point)
        return;
    if (changeFocusCustomPoint(point)) {
        d->focusPoint = point;
        emit customFocusPointChanged(point);
    }

}

QCameraFocusZoneList QWinRTCameraFocusControl::focusZones() const
{
    return QCameraFocusZoneList();
}

void QWinRTCameraFocusControl::setSupportedFocusMode(QCameraFocus::FocusModes modes)
{
    Q_D(QWinRTCameraFocusControl);
    d->supportedFocusModes = modes;
    d->focusModeInitialized = true;
    if (isFocusModeSupported(d->focusModes))
        return;
    d->focusModes = 0;
    if (!modes) {
        emit focusModeChanged(d->focusModes);
        return;
    }
    if (isFocusModeSupported(QCameraFocus::ContinuousFocus))
        d->focusModes = QCameraFocus::ContinuousFocus;
    else if (isFocusModeSupported(QCameraFocus::AutoFocus))
        d->focusModes = QCameraFocus::AutoFocus;
    else if (isFocusModeSupported(QCameraFocus::ManualFocus))
        d->focusModes = QCameraFocus::ManualFocus;
    emit focusModeChanged(d->focusModes);
}

void QWinRTCameraFocusControl::setSupportedFocusPointMode(const QSet<QCameraFocus::FocusPointMode> &supportedFocusPointModes)
{
    Q_D(QWinRTCameraFocusControl);
    d->supportedFocusPointModes = supportedFocusPointModes;
    d->focusPointModeInitialized = true;

    if (supportedFocusPointModes.isEmpty()) {
        if (d->focusPointMode != QCameraFocus::FocusPointAuto) {
            d->focusPointMode = QCameraFocus::FocusPointAuto;
            emit focusPointModeChanged(d->focusPointMode);
        }
        return;
    }

    if (isFocusPointModeSupported(d->focusPointMode))
        return;

    if (isFocusPointModeSupported(QCameraFocus::FocusPointCenter))
        d->focusPointMode = QCameraFocus::FocusPointCenter;
    else if (isFocusPointModeSupported(QCameraFocus::FocusPointAuto))
        d->focusPointMode = QCameraFocus::FocusPointAuto;
    else if (isFocusPointModeSupported(QCameraFocus::FocusPointCustom))
        d->focusPointMode = QCameraFocus::FocusPointCustom;
    else if (isFocusPointModeSupported(QCameraFocus::FocusPointFaceDetection))
        d->focusPointMode = QCameraFocus::FocusPointFaceDetection;
    emit focusPointModeChanged(d->focusPointMode);
}

void QWinRTCameraFocusControl::imageCaptureQueueChanged(bool isEmpty)
{
    Q_D(QWinRTCameraFocusControl);
    d->imageCaptureIdle = isEmpty;
}

bool QWinRTCameraFocusControl::changeFocusCustomPoint(const QPointF &point)
{
    Q_D(QWinRTCameraFocusControl);
    if (!d->focusPointModeInitialized || point.isNull())
        return true;
    QWinRTCameraControl *cameraControl = static_cast<QWinRTCameraControl *>(parent());
    Q_ASSERT(cameraControl);
    cameraControl->clearFocusPoint();
    return cameraControl->setFocusPoint(point);
}

QT_END_NAMESPACE
