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

#include "audioencodercontrol.h"
#include "audiocapturesession.h"

#include <qaudioformat.h>

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

static QAudioFormat audioSettingsToAudioFormat(const QAudioEncoderSettings &settings)
{
    QAudioFormat fmt;
    fmt.setCodec(settings.codec());
    fmt.setChannelCount(settings.channelCount());
    fmt.setSampleRate(settings.sampleRate());
    if (settings.sampleRate() == 8000 && settings.bitRate() == 8000) {
        fmt.setSampleType(QAudioFormat::UnSignedInt);
        fmt.setSampleSize(8);
    } else {
        fmt.setSampleSize(16);
        fmt.setSampleType(QAudioFormat::SignedInt);
    }
    fmt.setByteOrder(QAudioDeviceInfo::defaultInputDevice().preferredFormat().byteOrder());
    return fmt;
}

static QAudioEncoderSettings audioFormatToAudioSettings(const QAudioFormat &format)
{
    QAudioEncoderSettings settings;
    settings.setCodec(format.codec());
    settings.setChannelCount(format.channelCount());
    settings.setSampleRate(format.sampleRate());
    settings.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    settings.setBitRate(format.channelCount()
                        * format.sampleSize()
                        * format.sampleRate());
    return settings;
}

AudioEncoderControl::AudioEncoderControl(QObject *parent)
    :QAudioEncoderSettingsControl(parent)
{
    m_session = qobject_cast<AudioCaptureSession*>(parent);
    update();
}

AudioEncoderControl::~AudioEncoderControl()
{
}

QStringList AudioEncoderControl::supportedAudioCodecs() const
{
    return QStringList() << QStringLiteral("audio/pcm");
}

QString AudioEncoderControl::codecDescription(const QString &codecName) const
{
    if (QString::compare(codecName, QLatin1String("audio/pcm")) == 0)
        return tr("Linear PCM audio data");

    return QString();
}

QList<int> AudioEncoderControl::supportedSampleRates(const QAudioEncoderSettings &settings, bool *continuous) const
{
    if (continuous)
        *continuous = false;

    if (settings.codec().isEmpty() || settings.codec() == QLatin1String("audio/pcm"))
        return m_sampleRates;

    return QList<int>();
}

QAudioEncoderSettings AudioEncoderControl::audioSettings() const
{
    return audioFormatToAudioSettings(m_session->format());
}

void AudioEncoderControl::setAudioSettings(const QAudioEncoderSettings &settings)
{
    QAudioFormat fmt = audioSettingsToAudioFormat(settings);

    if (settings.encodingMode() == QMultimedia::ConstantQualityEncoding) {
        fmt.setCodec("audio/pcm");
        switch (settings.quality()) {
        case QMultimedia::VeryLowQuality:
            fmt.setSampleSize(8);
            fmt.setSampleRate(8000);
            fmt.setSampleType(QAudioFormat::UnSignedInt);
            break;
        case QMultimedia::LowQuality:
            fmt.setSampleSize(8);
            fmt.setSampleRate(22050);
            fmt.setSampleType(QAudioFormat::UnSignedInt);
            break;
        case QMultimedia::HighQuality:
            fmt.setSampleSize(16);
            fmt.setSampleRate(48000);
            fmt.setSampleType(QAudioFormat::SignedInt);
            break;
        case QMultimedia::VeryHighQuality:
            fmt.setSampleSize(16);
            fmt.setSampleRate(96000);
            fmt.setSampleType(QAudioFormat::SignedInt);
            break;
        case QMultimedia::NormalQuality:
        default:
            fmt.setSampleSize(16);
            fmt.setSampleRate(44100);
            fmt.setSampleType(QAudioFormat::SignedInt);
            break;
        }
    }

    m_session->setFormat(fmt);
}

void AudioEncoderControl::update()
{
    m_sampleRates.clear();
    QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for (int i = 0; i < devices.size(); ++i) {
        QList<int> rates = devices.at(i).supportedSampleRates();
        for (int j = 0; j < rates.size(); ++j) {
            int rate = rates.at(j);
            if (!m_sampleRates.contains(rate))
                m_sampleRates.append(rate);
        }
    }
    qSort(m_sampleRates);
}

QT_END_NAMESPACE
