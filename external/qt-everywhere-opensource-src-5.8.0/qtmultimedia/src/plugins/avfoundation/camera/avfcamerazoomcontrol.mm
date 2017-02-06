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

#include "avfcamerazoomcontrol.h"
#include "avfcameraservice.h"
#include "avfcamerautility.h"
#include "avfcamerasession.h"
#include "avfcameracontrol.h"
#include "avfcameradebug.h"

#include <QtCore/qsysinfo.h>
#include <QtCore/qglobal.h>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

AVFCameraZoomControl::AVFCameraZoomControl(AVFCameraService *service)
    : m_session(service->session()),
      m_maxZoomFactor(1.),
      m_zoomFactor(1.),
      m_requestedZoomFactor(1.)
{
    Q_ASSERT(m_session);
    connect(m_session, SIGNAL(stateChanged(QCamera::State)),
            SLOT(cameraStateChanged()));
}

qreal AVFCameraZoomControl::maximumOpticalZoom() const
{
    // Not supported.
    return 1.;
}

qreal AVFCameraZoomControl::maximumDigitalZoom() const
{
    return m_maxZoomFactor;
}

qreal AVFCameraZoomControl::requestedOpticalZoom() const
{
    // Not supported.
    return 1;
}

qreal AVFCameraZoomControl::requestedDigitalZoom() const
{
    return m_requestedZoomFactor;
}

qreal AVFCameraZoomControl::currentOpticalZoom() const
{
    // Not supported.
    return 1.;
}

qreal AVFCameraZoomControl::currentDigitalZoom() const
{
    return m_zoomFactor;
}

void AVFCameraZoomControl::zoomTo(qreal optical, qreal digital)
{
    Q_UNUSED(optical)
    Q_UNUSED(digital)
#if QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_7_0)
    if (QSysInfo::MacintoshVersion < QSysInfo::MV_IOS_7_0)
        return;

    if (qFuzzyCompare(CGFloat(digital), m_requestedZoomFactor))
        return;

    m_requestedZoomFactor = digital;
    Q_EMIT requestedDigitalZoomChanged(digital);

    zoomToRequestedDigital();
#endif
}

void AVFCameraZoomControl::cameraStateChanged()
{
#if QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_7_0)
    if (QSysInfo::MacintoshVersion < QSysInfo::MV_IOS_7_0)
        return;

    const QCamera::State state = m_session->state();
    if (state != QCamera::ActiveState) {
        if (state == QCamera::UnloadedState && m_maxZoomFactor > 1.) {
            m_maxZoomFactor = 1.;
            Q_EMIT maximumDigitalZoomChanged(1.);
        }
        return;
    }

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice || !captureDevice.activeFormat) {
        qDebugCamera() << Q_FUNC_INFO << "camera state is active, but"
                       << "video capture device and/or active format is nil";
        return;
    }

    if (captureDevice.activeFormat.videoMaxZoomFactor > 1.) {
        if (!qFuzzyCompare(m_maxZoomFactor, captureDevice.activeFormat.videoMaxZoomFactor)) {
            m_maxZoomFactor = captureDevice.activeFormat.videoMaxZoomFactor;
            Q_EMIT maximumDigitalZoomChanged(m_maxZoomFactor);
        }
    } else if (!qFuzzyCompare(m_maxZoomFactor, CGFloat(1.))) {
        m_maxZoomFactor = 1.;

        Q_EMIT maximumDigitalZoomChanged(1.);
    }

    zoomToRequestedDigital();
#endif
}

void AVFCameraZoomControl::zoomToRequestedDigital()
{
#if QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_7_0)
    if (QSysInfo::MacintoshVersion < QSysInfo::MV_IOS_7_0)
        return;

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice || !captureDevice.activeFormat)
        return;

    if (qFuzzyCompare(captureDevice.activeFormat.videoMaxZoomFactor, CGFloat(1.)))
        return;

    const CGFloat clampedZoom = qBound(CGFloat(1.), m_requestedZoomFactor,
                                       captureDevice.activeFormat.videoMaxZoomFactor);
    const CGFloat deviceZoom = captureDevice.videoZoomFactor;
    if (qFuzzyCompare(clampedZoom, deviceZoom)) {
        // Nothing to set, but check if a signal must be emitted:
        if (!qFuzzyCompare(m_zoomFactor, deviceZoom)) {
            m_zoomFactor = deviceZoom;
            Q_EMIT currentDigitalZoomChanged(deviceZoom);
        }
        return;
    }

    const AVFConfigurationLock lock(captureDevice);
    if (!lock) {
        qDebugCamera() << Q_FUNC_INFO << "failed to lock for configuration";
        return;
    }

    captureDevice.videoZoomFactor = clampedZoom;

    if (!qFuzzyCompare(clampedZoom, m_zoomFactor)) {
        m_zoomFactor = clampedZoom;
        Q_EMIT currentDigitalZoomChanged(clampedZoom);
    }
#endif
}

QT_END_NAMESPACE

#include "moc_avfcamerazoomcontrol.cpp"
