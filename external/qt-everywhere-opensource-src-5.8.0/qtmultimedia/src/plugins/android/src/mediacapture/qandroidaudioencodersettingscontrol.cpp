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

#include "qandroidaudioencodersettingscontrol.h"

#include "qandroidcapturesession.h"

QT_BEGIN_NAMESPACE

QAndroidAudioEncoderSettingsControl::QAndroidAudioEncoderSettingsControl(QAndroidCaptureSession *session)
    : QAudioEncoderSettingsControl()
    , m_session(session)
{
}

QStringList QAndroidAudioEncoderSettingsControl::supportedAudioCodecs() const
{
    return QStringList() << QLatin1String("amr-nb") << QLatin1String("amr-wb") << QLatin1String("aac");
}

QString QAndroidAudioEncoderSettingsControl::codecDescription(const QString &codecName) const
{
    if (codecName == QLatin1String("amr-nb"))
        return tr("Adaptive Multi-Rate Narrowband (AMR-NB) audio codec");
    else if (codecName == QLatin1String("amr-wb"))
        return tr("Adaptive Multi-Rate Wideband (AMR-WB) audio codec");
    else if (codecName == QLatin1String("aac"))
        return tr("AAC Low Complexity (AAC-LC) audio codec");

    return QString();
}

QList<int> QAndroidAudioEncoderSettingsControl::supportedSampleRates(const QAudioEncoderSettings &settings, bool *continuous) const
{
    if (continuous)
        *continuous = false;

    if (settings.isNull() || settings.codec().isNull() || settings.codec() == QLatin1String("aac")) {
        return QList<int>() << 8000 << 11025 << 12000 << 16000 << 22050
                            << 24000 << 32000 << 44100 << 48000 << 96000;
    } else if (settings.codec() == QLatin1String("amr-nb")) {
        return QList<int>() << 8000;
    } else if (settings.codec() == QLatin1String("amr-wb")) {
        return QList<int>() << 16000;
    }

    return QList<int>();
}

QAudioEncoderSettings QAndroidAudioEncoderSettingsControl::audioSettings() const
{
    return m_session->audioSettings();
}

void QAndroidAudioEncoderSettingsControl::setAudioSettings(const QAudioEncoderSettings &settings)
{
    m_session->setAudioSettings(settings);
}

QT_END_NAMESPACE
