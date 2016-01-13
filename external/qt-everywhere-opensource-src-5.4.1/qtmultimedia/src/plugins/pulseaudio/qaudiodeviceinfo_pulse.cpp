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

#include "qaudiodeviceinfo_pulse.h"
#include "qpulseaudioengine.h"
#include "qpulsehelpers.h"

QT_BEGIN_NAMESPACE

QPulseAudioDeviceInfo::QPulseAudioDeviceInfo(const QByteArray &device, QAudio::Mode mode)
    : m_device(device)
    , m_mode(mode)
{
}

bool QPulseAudioDeviceInfo::isFormatSupported(const QAudioFormat &format) const
{
    pa_sample_spec spec = QPulseAudioInternal::audioFormatToSampleSpec(format);
    if (!pa_sample_spec_valid(&spec))
        return false;

    return true;
}

QAudioFormat QPulseAudioDeviceInfo::preferredFormat() const
{
    QPulseAudioEngine *pulseEngine = QPulseAudioEngine::instance();
    QAudioFormat format = pulseEngine->m_preferredFormats.value(m_device);
    return format;
}

QString QPulseAudioDeviceInfo::deviceName() const
{
    return m_device;
}

QStringList QPulseAudioDeviceInfo::supportedCodecs()
{
    return QStringList() << "audio/pcm";
}

QList<int> QPulseAudioDeviceInfo::supportedSampleRates()
{
    return QList<int>() << 8000 << 11025 << 22050 << 44100 << 48000;
}

QList<int> QPulseAudioDeviceInfo::supportedChannelCounts()
{
    return QList<int>() << 1 << 2 << 4 << 6 << 8;
}

QList<int> QPulseAudioDeviceInfo::supportedSampleSizes()
{
    return QList<int>() << 8 << 16 << 24 << 32;
}

QList<QAudioFormat::Endian> QPulseAudioDeviceInfo::supportedByteOrders()
{
    return QList<QAudioFormat::Endian>() << QAudioFormat::BigEndian << QAudioFormat::LittleEndian;
}

QList<QAudioFormat::SampleType> QPulseAudioDeviceInfo::supportedSampleTypes()
{
    return QList<QAudioFormat::SampleType>() << QAudioFormat::SignedInt << QAudioFormat::UnSignedInt << QAudioFormat::Float;
}

QT_END_NAMESPACE
