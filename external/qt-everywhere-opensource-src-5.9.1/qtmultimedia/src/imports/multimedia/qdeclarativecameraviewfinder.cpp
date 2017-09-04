/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qdeclarativecamera_p.h"
#include "qdeclarativecameraviewfinder_p.h"

#include <QtMultimedia/qcameraviewfindersettingscontrol.h>


QT_BEGIN_NAMESPACE

QDeclarativeCameraViewfinder::QDeclarativeCameraViewfinder(QCamera *camera, QObject *parent)
    : QObject(parent)
    , m_camera(camera)
{
    connect(m_camera, &QCamera::statusChanged,
            this, &QDeclarativeCameraViewfinder::_q_cameraStatusChanged);
}

QDeclarativeCameraViewfinder::~QDeclarativeCameraViewfinder()
{
}

QSize QDeclarativeCameraViewfinder::resolution() const
{
    return m_settings.resolution();
}

void QDeclarativeCameraViewfinder::setResolution(const QSize &res)
{
    if (res != m_settings.resolution()) {
        m_settings = m_camera->viewfinderSettings();
        m_settings.setResolution(res);
        m_camera->setViewfinderSettings(m_settings);
        emit resolutionChanged();
    }
}

qreal QDeclarativeCameraViewfinder::minimumFrameRate() const
{
    return m_settings.minimumFrameRate();
}

void QDeclarativeCameraViewfinder::setMinimumFrameRate(qreal frameRate)
{
    if (frameRate != minimumFrameRate()) {
        m_settings = m_camera->viewfinderSettings();
        m_settings.setMinimumFrameRate(frameRate);
        m_camera->setViewfinderSettings(m_settings);
        emit minimumFrameRateChanged();
    }
}

qreal QDeclarativeCameraViewfinder::maximumFrameRate() const
{
    return m_settings.maximumFrameRate();
}

void QDeclarativeCameraViewfinder::setMaximumFrameRate(qreal frameRate)
{
    if (frameRate != maximumFrameRate()) {
        m_settings = m_camera->viewfinderSettings();
        m_settings.setMaximumFrameRate(frameRate);
        m_camera->setViewfinderSettings(m_settings);
        emit maximumFrameRateChanged();
    }
}

void QDeclarativeCameraViewfinder::_q_cameraStatusChanged(QCamera::Status status)
{
    // Settings values might change when the camera starts, for example if the settings are
    // undefined, if unsupported values were set or if the settings conflict with capture settings.
    // They might also change on LoadedStatus, for example reverting to values that were set by the
    // user.
    if (status == QCamera::ActiveStatus || status == QCamera::LoadedStatus) {
        QCameraViewfinderSettings oldSettings = m_settings;
        m_settings = m_camera->viewfinderSettings();
        if (oldSettings.resolution() != m_settings.resolution())
            emit resolutionChanged();
        if (oldSettings.minimumFrameRate() != m_settings.minimumFrameRate())
            emit minimumFrameRateChanged();
        if (oldSettings.maximumFrameRate() != m_settings.maximumFrameRate())
            emit maximumFrameRateChanged();
    }
}

QT_END_NAMESPACE

#include "moc_qdeclarativecameraviewfinder_p.cpp"
