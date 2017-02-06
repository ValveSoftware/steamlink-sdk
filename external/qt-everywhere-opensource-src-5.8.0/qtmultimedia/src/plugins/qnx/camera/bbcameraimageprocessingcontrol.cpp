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
#include "bbcameraimageprocessingcontrol.h"

#include "bbcamerasession.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

BbCameraImageProcessingControl::BbCameraImageProcessingControl(BbCameraSession *session, QObject *parent)
    : QCameraImageProcessingControl(parent)
    , m_session(session)
{
}

bool BbCameraImageProcessingControl::isParameterSupported(ProcessingParameter parameter) const
{
    return (parameter == QCameraImageProcessingControl::WhiteBalancePreset);
}

bool BbCameraImageProcessingControl::isParameterValueSupported(ProcessingParameter parameter, const QVariant &value) const
{
    if (parameter != QCameraImageProcessingControl::WhiteBalancePreset)
        return false;

    if (m_session->handle() == CAMERA_HANDLE_INVALID)
        return false;

    int supported = 0;
    camera_whitebalancemode_t modes[20];
    const camera_error_t result = camera_get_whitebalance_modes(m_session->handle(), 20, &supported, modes);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to retrieve supported whitebalance modes:" << result;
        return false;
    }

    QSet<QCameraImageProcessing::WhiteBalanceMode> supportedModes;
    for (int i = 0; i < supported; ++i) {
        switch (modes[i]) {
        case CAMERA_WHITEBALANCEMODE_AUTO:
            supportedModes.insert(QCameraImageProcessing::WhiteBalanceAuto);
            break;
        case CAMERA_WHITEBALANCEMODE_MANUAL:
            supportedModes.insert(QCameraImageProcessing::WhiteBalanceManual);
            break;
        default:
            break;
        }
    }

    return supportedModes.contains(value.value<QCameraImageProcessing::WhiteBalanceMode>());
}

QVariant BbCameraImageProcessingControl::parameter(ProcessingParameter parameter) const
{
    if (parameter != QCameraImageProcessingControl::WhiteBalancePreset)
        return QVariant();

    if (m_session->handle() == CAMERA_HANDLE_INVALID)
        return QVariant();

    camera_whitebalancemode_t mode;
    const camera_error_t result = camera_get_whitebalance_mode(m_session->handle(), &mode);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to retrieve current whitebalance mode:" << result;
        return QVariant();
    }

    switch (mode) {
    case CAMERA_WHITEBALANCEMODE_AUTO:
        return QVariant::fromValue(QCameraImageProcessing::WhiteBalanceAuto);
    case CAMERA_WHITEBALANCEMODE_MANUAL:
        return QVariant::fromValue(QCameraImageProcessing::WhiteBalanceManual);
    default:
        return QVariant();
    }
}

void BbCameraImageProcessingControl::setParameter(ProcessingParameter parameter, const QVariant &value)
{
    if (parameter != QCameraImageProcessingControl::WhiteBalancePreset)
        return;

    if (m_session->handle() == CAMERA_HANDLE_INVALID)
        return;

    camera_whitebalancemode_t mode = CAMERA_WHITEBALANCEMODE_DEFAULT;
    switch (value.value<QCameraImageProcessing::WhiteBalanceMode>()) {
    case QCameraImageProcessing::WhiteBalanceAuto:
        mode = CAMERA_WHITEBALANCEMODE_AUTO;
        break;
    case QCameraImageProcessing::WhiteBalanceManual:
        mode = CAMERA_WHITEBALANCEMODE_MANUAL;
        break;
    default:
        break;
    }

    const camera_error_t result = camera_set_whitebalance_mode(m_session->handle(), mode);

    if (result != CAMERA_EOK)
        qWarning() << "Unable to set whitebalance mode:" << result;
}

QT_END_NAMESPACE
