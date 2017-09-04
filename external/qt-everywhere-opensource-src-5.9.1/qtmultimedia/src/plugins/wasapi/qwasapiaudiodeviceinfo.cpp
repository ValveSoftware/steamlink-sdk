/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "qwasapiaudiodeviceinfo.h"
#include "qwasapiutils.h"

#include <Audioclient.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcMmDeviceInfo, "qt.multimedia.deviceinfo")

QWasapiAudioDeviceInfo::QWasapiAudioDeviceInfo(QByteArray dev, QAudio::Mode mode)
    : m_deviceName(dev)
{
    qCDebug(lcMmDeviceInfo) << __FUNCTION__ << dev << mode;
    m_interface = QWasapiUtils::createOrGetInterface(dev, mode);

    QAudioFormat referenceFormat = m_interface->m_mixFormat;

    const int rates[] = {8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 192000};
    for (int rate : rates) {
        QAudioFormat f = referenceFormat;
        f.setSampleRate(rate);
        if (isFormatSupported(f))
            m_sampleRates.append(rate);
    }

    for (int i = 1; i <= 18; ++i) {
        QAudioFormat f = referenceFormat;
        f.setChannelCount(i);
        if (isFormatSupported(f))
            m_channelCounts.append(i);
    }

    const int sizes[] = {8, 12, 16, 20, 24, 32, 64};
    for (int s : sizes) {
        QAudioFormat f = referenceFormat;
        f.setSampleSize(s);
        if (isFormatSupported(f))
            m_sampleSizes.append(s);
    }

    referenceFormat.setSampleType(QAudioFormat::SignedInt);
    if (isFormatSupported(referenceFormat))
        m_sampleTypes.append(QAudioFormat::SignedInt);

    referenceFormat.setSampleType(QAudioFormat::Float);
    if (isFormatSupported(referenceFormat))
        m_sampleTypes.append(QAudioFormat::Float);
}

QWasapiAudioDeviceInfo::~QWasapiAudioDeviceInfo()
{
    qCDebug(lcMmDeviceInfo) << __FUNCTION__;
}

bool QWasapiAudioDeviceInfo::isFormatSupported(const QAudioFormat& format) const
{
    qCDebug(lcMmDeviceInfo) << __FUNCTION__ << format;

    WAVEFORMATEX nfmt;
    if (!QWasapiUtils::convertToNativeFormat(format, &nfmt))
        return false;

    WAVEFORMATEX closest;
    WAVEFORMATEX *pClosest = &closest;
    HRESULT hr;
    hr = m_interface->m_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &nfmt, &pClosest);

    if (hr == S_OK) // S_FALSE is inside SUCCEEDED()
        return true;

    if (lcMmDeviceInfo().isDebugEnabled()) {
        QAudioFormat f;
        QWasapiUtils::convertFromNativeFormat(pClosest, &f);
        qCDebug(lcMmDeviceInfo) << __FUNCTION__ << hr << "Closest match is:" << f;
    }

    return false;
}

QAudioFormat QWasapiAudioDeviceInfo::preferredFormat() const
{
    qCDebug(lcMmDeviceInfo) << __FUNCTION__;
    return m_interface->m_mixFormat;
}

QString QWasapiAudioDeviceInfo::deviceName() const
{
    qCDebug(lcMmDeviceInfo) << __FUNCTION__;
    return m_deviceName;
}

QStringList QWasapiAudioDeviceInfo::supportedCodecs()
{
    qCDebug(lcMmDeviceInfo) << __FUNCTION__;
    return QStringList() << QStringLiteral("audio/pcm");
}

QList<int> QWasapiAudioDeviceInfo::supportedSampleRates()
{
    qCDebug(lcMmDeviceInfo) << __FUNCTION__;
    return m_sampleRates;
}

QList<int> QWasapiAudioDeviceInfo::supportedChannelCounts()
{
    qCDebug(lcMmDeviceInfo) << __FUNCTION__;
    return m_channelCounts;
}

QList<int> QWasapiAudioDeviceInfo::supportedSampleSizes()
{
    qCDebug(lcMmDeviceInfo) << __FUNCTION__;
    return m_sampleSizes;
}

QList<QAudioFormat::Endian> QWasapiAudioDeviceInfo::supportedByteOrders()
{
    qCDebug(lcMmDeviceInfo) << __FUNCTION__;
    return QList<QAudioFormat::Endian>() << m_interface->m_mixFormat.byteOrder();
}

QList<QAudioFormat::SampleType> QWasapiAudioDeviceInfo::supportedSampleTypes()
{
    qCDebug(lcMmDeviceInfo) << __FUNCTION__;
    return m_sampleTypes;
}

QT_END_NAMESPACE
