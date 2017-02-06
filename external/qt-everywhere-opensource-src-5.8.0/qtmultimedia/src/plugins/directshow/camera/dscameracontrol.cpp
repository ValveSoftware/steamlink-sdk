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

#include <QtCore/qdebug.h>

#include "dscameracontrol.h"
#include "dscameraservice.h"
#include "dscamerasession.h"

QT_BEGIN_NAMESPACE

DSCameraControl::DSCameraControl(QObject *parent)
    : QCameraControl(parent)
    , m_state(QCamera::UnloadedState)
    , m_captureMode(QCamera::CaptureStillImage)
{
    m_session = qobject_cast<DSCameraSession*>(parent);
    connect(m_session, SIGNAL(statusChanged(QCamera::Status)),
            this, SIGNAL(statusChanged(QCamera::Status)));
}

DSCameraControl::~DSCameraControl()
{
}

void DSCameraControl::setState(QCamera::State state)
{
    if (m_state == state)
        return;

    bool succeeded = false;
    switch (state) {
    case QCamera::UnloadedState:
        succeeded = m_session->unload();
        break;
    case QCamera::LoadedState:
    case QCamera::ActiveState:
        if (m_state == QCamera::UnloadedState && !m_session->load())
            return;

        if (state == QCamera::ActiveState)
            succeeded = m_session->startPreview();
        else
            succeeded = m_session->stopPreview();

        break;
    }

    if (succeeded) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

bool DSCameraControl::isCaptureModeSupported(QCamera::CaptureModes mode) const
{
    bool bCaptureSupported = false;
    switch (mode) {
    case QCamera::CaptureStillImage:
        bCaptureSupported = true;
        break;
    case QCamera::CaptureVideo:
        bCaptureSupported = false;
        break;
    }
    return bCaptureSupported;
}

void DSCameraControl::setCaptureMode(QCamera::CaptureModes mode)
{
    if (m_captureMode != mode && isCaptureModeSupported(mode)) {
        m_captureMode = mode;
        emit captureModeChanged(mode);
    }
}

QCamera::Status DSCameraControl::status() const
{
    return m_session->status();
}

QT_END_NAMESPACE
