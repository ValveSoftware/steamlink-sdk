/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "qandroidcameracontrol.h"

#include "qandroidcamerasession.h"

QT_BEGIN_NAMESPACE

QAndroidCameraControl::QAndroidCameraControl(QAndroidCameraSession *session)
    : QCameraControl(0)
    , m_cameraSession(session)

{
    connect(m_cameraSession, SIGNAL(statusChanged(QCamera::Status)),
            this, SIGNAL(statusChanged(QCamera::Status)));

    connect(m_cameraSession, SIGNAL(stateChanged(QCamera::State)),
            this, SIGNAL(stateChanged(QCamera::State)));

    connect(m_cameraSession, SIGNAL(error(int,QString)), this, SIGNAL(error(int,QString)));

    connect(m_cameraSession, SIGNAL(captureModeChanged(QCamera::CaptureModes)),
            this, SIGNAL(captureModeChanged(QCamera::CaptureModes)));
}

QAndroidCameraControl::~QAndroidCameraControl()
{
}

QCamera::CaptureModes QAndroidCameraControl::captureMode() const
{
    return m_cameraSession->captureMode();
}

void QAndroidCameraControl::setCaptureMode(QCamera::CaptureModes mode)
{
    m_cameraSession->setCaptureMode(mode);
}

bool QAndroidCameraControl::isCaptureModeSupported(QCamera::CaptureModes mode) const
{
    return m_cameraSession->isCaptureModeSupported(mode);
}

void QAndroidCameraControl::setState(QCamera::State state)
{
    m_cameraSession->setState(state);
}

QCamera::State QAndroidCameraControl::state() const
{
    return m_cameraSession->state();
}

QCamera::Status QAndroidCameraControl::status() const
{
    return m_cameraSession->status();
}

bool QAndroidCameraControl::canChangeProperty(PropertyChangeType changeType, QCamera::Status status) const
{
    Q_UNUSED(status);

    switch (changeType) {
    case QCameraControl::CaptureMode:
    case QCameraControl::ImageEncodingSettings:
    case QCameraControl::VideoEncodingSettings:
    case QCameraControl::Viewfinder:
        return true;
    default:
        return false;
    }
}

QT_END_NAMESPACE
