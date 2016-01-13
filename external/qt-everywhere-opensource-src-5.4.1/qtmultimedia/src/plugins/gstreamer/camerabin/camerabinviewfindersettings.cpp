/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd.
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


#include "camerabinviewfindersettings.h"


QT_BEGIN_NAMESPACE

CameraBinViewfinderSettings::CameraBinViewfinderSettings(QObject *parent)
    : QCameraViewfinderSettingsControl(parent),
      m_minimumFrameRate(0),
      m_maximumFrameRate(0)
{
}

CameraBinViewfinderSettings::~CameraBinViewfinderSettings()
{
}

bool CameraBinViewfinderSettings::isViewfinderParameterSupported(ViewfinderParameter parameter) const
{
    switch (parameter) {
    case Resolution:
    case MinimumFrameRate:
    case MaximumFrameRate:
        return true;
    case PixelAspectRatio:
    case PixelFormat:
    case UserParameter:
        return false;
    }
    return false;
}

QVariant CameraBinViewfinderSettings::viewfinderParameter(ViewfinderParameter parameter) const
{
    switch (parameter) {
    case Resolution:
        return m_resolution;
    case MinimumFrameRate:
        return m_minimumFrameRate;
    case MaximumFrameRate:
        return m_maximumFrameRate;
    case PixelAspectRatio:
    case PixelFormat:
    case UserParameter:
        return QVariant();
    }
    return false;
}

void CameraBinViewfinderSettings::setViewfinderParameter(ViewfinderParameter parameter, const QVariant &value)
{
    switch (parameter) {
    case Resolution:
        m_resolution = value.toSize();
        break;
    case MinimumFrameRate:
        m_minimumFrameRate = value.toFloat();
        break;
    case MaximumFrameRate:
        m_maximumFrameRate = value.toFloat();
        break;
    case PixelAspectRatio:
    case PixelFormat:
    case UserParameter:
        break;
    }
}

QSize CameraBinViewfinderSettings::resolution() const
{
    return m_resolution;
}

qreal CameraBinViewfinderSettings::minimumFrameRate() const
{
    return m_minimumFrameRate;
}

qreal CameraBinViewfinderSettings::maximumFrameRate() const
{
    return m_maximumFrameRate;
}

QT_END_NAMESPACE
