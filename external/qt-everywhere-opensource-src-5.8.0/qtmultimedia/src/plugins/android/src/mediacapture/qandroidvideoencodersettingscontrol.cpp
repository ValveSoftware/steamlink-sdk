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
