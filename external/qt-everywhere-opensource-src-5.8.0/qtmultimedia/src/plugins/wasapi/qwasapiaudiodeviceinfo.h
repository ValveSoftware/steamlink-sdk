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

#ifndef QWASAPIAUDIODEVICEINFO_H
#define QWASAPIAUDIODEVICEINFO_H

#include <QtCore/QList>
#include <QtCore/QLoggingCategory>
#include <QtCore/QStringList>
#include <QtMultimedia/QAbstractAudioDeviceInfo>
#include <QtMultimedia/QAudio>
#include <QtMultimedia/QAudioFormat>

#include <wrl.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcMmDeviceInfo)

class AudioInterface;
class QWasapiAudioDeviceInfo : public QAbstractAudioDeviceInfo
{
    Q_OBJECT
public:
    explicit QWasapiAudioDeviceInfo(QByteArray dev,QAudio::Mode mode);
    ~QWasapiAudioDeviceInfo();

    QAudioFormat preferredFormat() const Q_DECL_OVERRIDE;
    bool isFormatSupported(const QAudioFormat& format) const Q_DECL_OVERRIDE;
    QString deviceName() const Q_DECL_OVERRIDE;
    QStringList supportedCodecs() Q_DECL_OVERRIDE;
    QList<int> supportedSampleRates() Q_DECL_OVERRIDE;
    QList<int> supportedChannelCounts() Q_DECL_OVERRIDE;
    QList<int> supportedSampleSizes() Q_DECL_OVERRIDE;
    QList<QAudioFormat::Endian> supportedByteOrders() Q_DECL_OVERRIDE;
    QList<QAudioFormat::SampleType> supportedSampleTypes() Q_DECL_OVERRIDE;

private:
    Microsoft::WRL::ComPtr<AudioInterface> m_interface;
    QString m_deviceName;
    QList<int> m_sampleRates;
    QList<int> m_channelCounts;
    QList<int> m_sampleSizes;
    QList<QAudioFormat::SampleType> m_sampleTypes;
};

QT_END_NAMESPACE

#endif // QWASAPIAUDIODEVICEINFO_H
