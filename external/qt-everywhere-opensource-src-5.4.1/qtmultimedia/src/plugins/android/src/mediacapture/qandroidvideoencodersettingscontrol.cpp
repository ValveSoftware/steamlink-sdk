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

#include "qandroidvideoencodersettingscontrol.h"

#include "qandroidcapturesession.h"

QT_BEGIN_NAMESPACE

QAndroidVideoEncoderSettingsControl::QAndroidVideoEncoderSettingsControl(QAndroidCaptureSession *session)
    : QVideoEncoderSettingsControl()
    , m_session(session)
{
}

QList<QSize> QAndroidVideoEncoderSettingsControl::supportedResolutions(const QVideoEncoderSettings &, bool *continuous) const
{
    if (continuous)
        *continuous = false;

    return m_session->supportedResolutions();
}

QList<qreal> QAndroidVideoEncoderSettingsControl::supportedFrameRates(const QVideoEncoderSettings &, bool *continuous) const
{
    if (continuous)
        *continuous = false;

    return m_session->supportedFrameRates();
}

QStringList QAndroidVideoEncoderSettingsControl::supportedVideoCodecs() const
{
    return QStringList() << QLatin1String("h263")
                         << QLatin1String("h264")
                         << QLatin1String("mpeg4_sp");
}

QString QAndroidVideoEncoderSettingsControl::videoCodecDescription(const QString &codecName) const
{
    if (codecName == QLatin1String("h263"))
        return tr("H.263 compression");
    else if (codecName == QLatin1String("h264"))
        return tr("H.264 compression");
    else if (codecName == QLatin1String("mpeg4_sp"))
        return tr("MPEG-4 SP compression");

    return QString();
}

QVideoEncoderSettings QAndroidVideoEncoderSettingsControl::videoSettings() const
{
    return m_session->videoSettings();
}

void QAndroidVideoEncoderSettingsControl::setVideoSettings(const QVideoEncoderSettings &settings)
{
    m_session->setVideoSettings(settings);
}

QT_END_NAMESPACE
