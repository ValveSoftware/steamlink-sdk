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
