/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd.
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


#include "camerabinviewfindersettings.h"
#include "camerabinsession.h"

QT_BEGIN_NAMESPACE


CameraBinViewfinderSettings::CameraBinViewfinderSettings(CameraBinSession *session)
    : QCameraViewfinderSettingsControl(session)
    , m_session(session)
{
}

CameraBinViewfinderSettings::~CameraBinViewfinderSettings()
{
}

bool CameraBinViewfinderSettings::isViewfinderParameterSupported(ViewfinderParameter parameter) const
{
    switch (parameter) {
    case Resolution:
    case PixelAspectRatio:
    case MinimumFrameRate:
    case MaximumFrameRate:
    case PixelFormat:
        return true;
    case UserParameter:
        return false;
    }
    return false;
}

QVariant CameraBinViewfinderSettings::viewfinderParameter(ViewfinderParameter parameter) const
{
    switch (parameter) {
    case Resolution:
        return m_session->viewfinderSettings().resolution();
    case PixelAspectRatio:
        return m_session->viewfinderSettings().pixelAspectRatio();
    case MinimumFrameRate:
        return m_session->viewfinderSettings().minimumFrameRate();
    case MaximumFrameRate:
        return m_session->viewfinderSettings().maximumFrameRate();
    case PixelFormat:
        return m_session->viewfinderSettings().pixelFormat();
    case UserParameter:
        return QVariant();
    }
    return false;
}

void CameraBinViewfinderSettings::setViewfinderParameter(ViewfinderParameter parameter, const QVariant &value)
{
    QCameraViewfinderSettings settings = m_session->viewfinderSettings();

    switch (parameter) {
    case Resolution:
        settings.setResolution(value.toSize());
        break;
    case PixelAspectRatio:
        settings.setPixelAspectRatio(value.toSize());
        break;
    case MinimumFrameRate:
        settings.setMinimumFrameRate(value.toReal());
        break;
    case MaximumFrameRate:
        settings.setMaximumFrameRate(value.toReal());
        break;
    case PixelFormat:
        settings.setPixelFormat(qvariant_cast<QVideoFrame::PixelFormat>(value));
    case UserParameter:
        break;
    }

    m_session->setViewfinderSettings(settings);
}

QT_END_NAMESPACE
