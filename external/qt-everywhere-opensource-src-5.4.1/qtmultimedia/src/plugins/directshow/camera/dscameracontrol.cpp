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
