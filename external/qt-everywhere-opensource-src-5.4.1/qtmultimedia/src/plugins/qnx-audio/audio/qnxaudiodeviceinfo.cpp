/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
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

#include "qnxaudiodeviceinfo.h"

#include "qnxaudioutils.h"

#include <sys/asoundlib.h>

QT_BEGIN_NAMESPACE

QnxAudioDeviceInfo::QnxAudioDeviceInfo(const QString &deviceName, QAudio::Mode mode)
    : m_name(deviceName),
      m_mode(mode)
{
}

QnxAudioDeviceInfo::~QnxAudioDeviceInfo()
{
}

QAudioFormat QnxAudioDeviceInfo::preferredFormat() const
{
    QAudioFormat format;
    if (m_mode == QAudio::AudioOutput) {
        format.setSampleRate(44100);
        format.setChannelCount(2);
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);
        format.setSampleSize(16);
        format.setCodec(QLatin1String("audio/pcm"));
    } else {
        format.setSampleRate(8000);
        format.setChannelCount(1);
        format.setSampleType(QAudioFormat::UnSignedInt);
        format.setSampleSize(8);
        format.setCodec(QLatin1String("audio/pcm"));
        if (!isFormatSupported(format)) {
            format.setChannelCount(2);
            format.setSampleSize(16);
            format.setSampleType(QAudioFormat::SignedInt);
        }
    }
    return format;
}

bool QnxAudioDeviceInfo::isFormatSupported(const QAudioFormat &format) const
{
    if (!format.codec().startsWith(QLatin1String("audio/pcm")))
        return false;

    const int pcmMode = (m_mode == QAudio::AudioOutput) ? SND_PCM_OPEN_PLAYBACK : SND_PCM_OPEN_CAPTURE;
    snd_pcm_t *handle;

    int card = 0;
    int device = 0;
    if (snd_pcm_open_preferred(&handle, &card, &device, pcmMode) < 0)
        return false;

    snd_pcm_channel_info_t info;
    memset (&info, 0, sizeof(info));
    info.channel = (m_mode == QAudio::AudioOutput) ? SND_PCM_CHANNEL_PLAYBACK : SND_PCM_CHANNEL_CAPTURE;

    if (snd_pcm_plugin_info(handle, &info) < 0) {
        qWarning("QAudioDeviceInfo: couldn't get channel info");
        snd_pcm_close(handle);
        return false;
    }

    snd_pcm_channel_params_t params = QnxAudioUtils::formatToChannelParams(format, m_mode, info.max_fragment_size);
    const int errorCode = snd_pcm_plugin_params(handle, &params);
    snd_pcm_close(handle);

    return errorCode == 0;
}

QString QnxAudioDeviceInfo::deviceName() const
{
    return m_name;
}

QStringList QnxAudioDeviceInfo::supportedCodecs()
{
    return QStringList() << QLatin1String("audio/pcm");
}

QList<int> QnxAudioDeviceInfo::supportedSampleRates()
{
    return QList<int>() << 8000 << 11025 << 22050 << 44100 << 48000;
}

QList<int> QnxAudioDeviceInfo::supportedChannelCounts()
{
    return QList<int>() << 1 << 2;
}

QList<int> QnxAudioDeviceInfo::supportedSampleSizes()
{
    return QList<int>() << 8 << 16 << 32;
}

QList<QAudioFormat::Endian> QnxAudioDeviceInfo::supportedByteOrders()
{
    return QList<QAudioFormat::Endian>() << QAudioFormat::LittleEndian << QAudioFormat::BigEndian;
}

QList<QAudioFormat::SampleType> QnxAudioDeviceInfo::supportedSampleTypes()
{
    return QList<QAudioFormat::SampleType>() << QAudioFormat::SignedInt << QAudioFormat::UnSignedInt << QAudioFormat::Float;
}

QT_END_NAMESPACE
