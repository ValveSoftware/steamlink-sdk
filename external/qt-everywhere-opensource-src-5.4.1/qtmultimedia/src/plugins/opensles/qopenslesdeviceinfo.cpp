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

#include "qopenslesdeviceinfo.h"

#include "qopenslesengine.h"

QT_BEGIN_NAMESPACE

QOpenSLESDeviceInfo::QOpenSLESDeviceInfo(const QByteArray &device, QAudio::Mode mode)
    : m_engine(QOpenSLESEngine::instance())
    , m_device(device)
    , m_mode(mode)
{
}

bool QOpenSLESDeviceInfo::isFormatSupported(const QAudioFormat &format) const
{
    QOpenSLESDeviceInfo *that = const_cast<QOpenSLESDeviceInfo*>(this);
    return that->supportedCodecs().contains(format.codec())
            && that->supportedSampleRates().contains(format.sampleRate())
            && that->supportedChannelCounts().contains(format.channelCount())
            && that->supportedSampleSizes().contains(format.sampleSize())
            && that->supportedByteOrders().contains(format.byteOrder())
            && that->supportedSampleTypes().contains(format.sampleType());
}

QAudioFormat QOpenSLESDeviceInfo::preferredFormat() const
{
    QAudioFormat format;
    format.setCodec(QStringLiteral("audio/pcm"));
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setSampleRate(44100);
    format.setChannelCount(m_mode == QAudio::AudioInput ? 1 : 2);
    return format;
}

QString QOpenSLESDeviceInfo::deviceName() const
{
    return m_device;
}

QStringList QOpenSLESDeviceInfo::supportedCodecs()
{
    return QStringList() << QStringLiteral("audio/pcm");
}

QList<int> QOpenSLESDeviceInfo::supportedSampleRates()
{
    return m_engine->supportedSampleRates(m_mode);
}

QList<int> QOpenSLESDeviceInfo::supportedChannelCounts()
{
    return m_engine->supportedChannelCounts(m_mode);
}

QList<int> QOpenSLESDeviceInfo::supportedSampleSizes()
{
    if (m_mode == QAudio::AudioInput)
        return QList<int>() << 16;
    else
        return QList<int>() << 8 << 16;
}

QList<QAudioFormat::Endian> QOpenSLESDeviceInfo::supportedByteOrders()
{
    return QList<QAudioFormat::Endian>() << QAudioFormat::LittleEndian;
}

QList<QAudioFormat::SampleType> QOpenSLESDeviceInfo::supportedSampleTypes()
{
    return QList<QAudioFormat::SampleType>() << QAudioFormat::SignedInt;
}

QT_END_NAMESPACE
