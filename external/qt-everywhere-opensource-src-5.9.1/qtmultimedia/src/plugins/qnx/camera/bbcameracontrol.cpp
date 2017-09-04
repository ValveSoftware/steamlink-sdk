/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#include "bbcameracontrol.h"

#include "bbcamerasession.h"

QT_BEGIN_NAMESPACE

BbCameraControl::BbCameraControl(BbCameraSession *session, QObject *parent)
    : QCameraControl(parent)
    , m_session(session)
{
    connect(m_session, SIGNAL(statusChanged(QCamera::Status)), this, SIGNAL(statusChanged(QCamera::Status)));
    connect(m_session, SIGNAL(stateChanged(QCamera::State)), this, SIGNAL(stateChanged(QCamera::State)));
    connect(m_session, SIGNAL(error(int,QString)), this, SIGNAL(error(int,QString)));
    connect(m_session, SIGNAL(captureModeChanged(QCamera::CaptureModes)), this, SIGNAL(captureModeChanged(QCamera::CaptureModes)));
}

QCamera::State BbCameraControl::state() const
{
    return m_session->state();
}

void BbCameraControl::setState(QCamera::State state)
{
    m_session->setState(state);
}

QCamera::CaptureModes BbCameraControl::captureMode() const
{
    return m_session->captureMode();
}

void BbCameraControl::setCaptureMode(QCamera::CaptureModes mode)
{
    m_session->setCaptureMode(mode);
}

QCamera::Status BbCameraControl::status() const
{
    return m_session->status();
}

bool BbCameraControl::isCaptureModeSupported(QCamera::CaptureModes mode) const
{
    return m_session->isCaptureModeSupported(mode);
}

bool BbCameraControl::canChangeProperty(PropertyChangeType /* changeType */, QCamera::Status /* status */) const
{
    return false;
}

QT_END_NAMESPACE
