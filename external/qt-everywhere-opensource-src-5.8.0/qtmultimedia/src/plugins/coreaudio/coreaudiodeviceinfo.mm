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

#include "coreaudiodeviceinfo.h"
#include "coreaudioutils.h"
#if defined(Q_OS_IOS) || defined(Q_OS_TVOS)
# include "coreaudiosessionmanager.h"
#endif

#include <QtCore/QDataStream>
#include <QtCore/QDebug>
#include <QtCore/QSet>

QT_BEGIN_NAMESPACE

CoreAudioDeviceInfo::CoreAudioDeviceInfo(const QByteArray &device, QAudio::Mode mode)
    : m_mode(mode)
{
#if defined(Q_OS_OSX)
    quint32 deviceID;

    QDataStream dataStream(device);
    dataStream >> deviceID >> m_device;
    m_deviceId = AudioDeviceID(deviceID);
#else //iOS
    m_device = device;
    if (mode == QAudio::AudioInput) {
        if (CoreAudioSessionManager::instance().category() != CoreAudioSessionManager::PlayAndRecord) {
            CoreAudioSessionManager::instance().setCategory(CoreAudioSessionManager::PlayAndRecord);
        }
    }
#endif
}


QAudioFormat CoreAudioDeviceInfo::preferredFormat() const
{
    QAudioFormat format;

#if defined(Q_OS_OSX)
    UInt32  propSize = 0;
    AudioObjectPropertyScope audioDevicePropertyScope = m_mode == QAudio::AudioInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    AudioObjectPropertyAddress audioDevicePropertyStreamsAddress = { kAudioDevicePropertyStreams,
                                                                     audioDevicePropertyScope,
                                                                     kAudioObjectPropertyElementMaster };

    if (AudioObjectGetPropertyDataSize(m_deviceId, &audioDevicePropertyStreamsAddress, 0, NULL, &propSize) == noErr) {

        const int sc = propSize / sizeof(AudioStreamID);

        if (sc > 0) {
            AudioStreamID*  streams = new AudioStreamID[sc];

            if (AudioObjectGetPropertyData(m_deviceId, &audioDevicePropertyStreamsAddress, 0, NULL, &propSize, streams) == noErr) {

                AudioObjectPropertyAddress audioDevicePhysicalFormatPropertyAddress = { kAudioStreamPropertyPhysicalFormat,
                                                                                        kAudioObjectPropertyScopeGlobal,
                                                                                        kAudioObjectPropertyElementMaster };

                for (int i = 0; i < sc; ++i) {
                    if (AudioObjectGetPropertyDataSize(streams[i], &audioDevicePhysicalFormatPropertyAddress, 0, NULL, &propSize) == noErr) {
                        AudioStreamBasicDescription sf;

                        if (AudioObjectGetPropertyData(streams[i], &audioDevicePhysicalFormatPropertyAddress, 0, NULL, &propSize, &sf) == noErr) {
                            format = CoreAudioUtils::toQAudioFormat(sf);
                            break;
                        } else {
                            qWarning() << "QAudioDeviceInfo: Unable to find perferedFormat for stream";
                        }
                    } else {
                        qWarning() << "QAudioDeviceInfo: Unable to find size of perferedFormat for stream";
                    }
                }
            }

            delete[] streams;
        }
    }
#else //iOS
    format.setSampleSize(16);
    if (m_mode == QAudio::AudioInput) {
        format.setChannelCount(1);
        format.setSampleRate(8000);
    } else {
        format.setChannelCount(2);
        format.setSampleRate(44100);
    }
    format.setCodec(QString::fromLatin1("audio/pcm"));
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);
#endif

    return format;
}


bool CoreAudioDeviceInfo::isFormatSupported(const QAudioFormat &format) const
{
    CoreAudioDeviceInfo *self = const_cast<CoreAudioDeviceInfo*>(this);

    //Sample rates are more of a suggestion with CoreAudio so as long as we get a
    //sane value then we can likely use it.
    return format.isValid()
            && format.codec() == QString::fromLatin1("audio/pcm")
            && format.sampleRate() > 0
            && self->supportedChannelCounts().contains(format.channelCount())
            && self->supportedSampleSizes().contains(format.sampleSize());
}


QString CoreAudioDeviceInfo::deviceName() const
{
    return m_device;
}


QStringList CoreAudioDeviceInfo::supportedCodecs()
{
    return QStringList() << QString::fromLatin1("audio/pcm");
}


QList<int> CoreAudioDeviceInfo::supportedSampleRates()
{
    QSet<int> sampleRates;

#if defined(Q_OS_OSX)
    UInt32  propSize = 0;
    AudioObjectPropertyScope scope = m_mode == QAudio::AudioInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    AudioObjectPropertyAddress availableNominalSampleRatesAddress = { kAudioDevicePropertyAvailableNominalSampleRates,
                                                                      scope,
                                                                      kAudioObjectPropertyElementMaster };

    if (AudioObjectGetPropertyDataSize(m_deviceId, &availableNominalSampleRatesAddress, 0, NULL, &propSize) == noErr) {
        const int pc = propSize / sizeof(AudioValueRange);

        if (pc > 0) {
            AudioValueRange* vr = new AudioValueRange[pc];

            if (AudioObjectGetPropertyData(m_deviceId, &availableNominalSampleRatesAddress, 0, NULL, &propSize, vr) == noErr) {
                for (int i = 0; i < pc; ++i) {
                    sampleRates << vr[i].mMinimum << vr[i].mMaximum;
                }
            }

            delete[] vr;
        }
    }
#else //iOS
    //iOS doesn't have a way to query available sample rates
    //instead we provide reasonable targets
    //It may be necessary have CoreAudioSessionManger test combinations
    //with available hardware
    sampleRates << 8000 << 11025 << 22050 << 44100 << 48000;
#endif
    return sampleRates.toList();
}


QList<int> CoreAudioDeviceInfo::supportedChannelCounts()
{
    static QList<int> supportedChannels;

    if (supportedChannels.isEmpty()) {
        // If the number of channels is not supported by an audio device, Core Audio will
        // automatically convert the audio data.
        for (int i = 1; i <= 16; ++i)
            supportedChannels.append(i);
    }

    return supportedChannels;
}


QList<int> CoreAudioDeviceInfo::supportedSampleSizes()
{
    return QList<int>() << 8 << 16 << 24 << 32 << 64;
}


QList<QAudioFormat::Endian> CoreAudioDeviceInfo::supportedByteOrders()
{
    return QList<QAudioFormat::Endian>() << QAudioFormat::LittleEndian << QAudioFormat::BigEndian;
}


QList<QAudioFormat::SampleType> CoreAudioDeviceInfo::supportedSampleTypes()
{
    return QList<QAudioFormat::SampleType>() << QAudioFormat::SignedInt << QAudioFormat::UnSignedInt << QAudioFormat::Float;
}

#if defined(Q_OS_OSX)
// XXX: remove at some future date
static inline QString cfStringToQString(CFStringRef str)
{
    CFIndex length = CFStringGetLength(str);
    const UniChar *chars = CFStringGetCharactersPtr(str);
    if (chars)
        return QString(reinterpret_cast<const QChar *>(chars), length);

    UniChar buffer[length];
    CFStringGetCharacters(str, CFRangeMake(0, length), buffer);
    return QString(reinterpret_cast<const QChar *>(buffer), length);
}

static QByteArray get_device_info(AudioDeviceID audioDevice, QAudio::Mode mode)
{
    UInt32      size;
    QByteArray  device;
    QDataStream ds(&device, QIODevice::WriteOnly);
    AudioStreamBasicDescription     sf;
    CFStringRef name;
    Boolean     isInput = mode == QAudio::AudioInput;
    AudioObjectPropertyScope audioPropertyScope = isInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

    // Id
    ds << quint32(audioDevice);

    // Mode //TODO: Why don't we use the Stream Format we ask for?
    size = sizeof(AudioStreamBasicDescription);
    AudioObjectPropertyAddress audioDeviceStreamFormatPropertyAddress = { kAudioDevicePropertyStreamFormat,
                                                                    audioPropertyScope,
                                                                    kAudioObjectPropertyElementMaster };

    if (AudioObjectGetPropertyData(audioDevice, &audioDeviceStreamFormatPropertyAddress, 0, NULL, &size, &sf) != noErr) {
        return QByteArray();
    }

    // Name
    size = sizeof(CFStringRef);
    AudioObjectPropertyAddress audioDeviceNamePropertyAddress = { kAudioObjectPropertyName,
                                                                  audioPropertyScope,
                                                                  kAudioObjectPropertyElementMaster };

    if (AudioObjectGetPropertyData(audioDevice, &audioDeviceNamePropertyAddress, 0, NULL, &size, &name) != noErr) {
        qWarning() << "QAudioDeviceInfo: Unable to find device name";
        return QByteArray();
    }
    ds << cfStringToQString(name);

    CFRelease(name);

    return device;
}
#endif

QByteArray CoreAudioDeviceInfo::defaultDevice(QAudio::Mode mode)
{
#if defined(Q_OS_OSX)
    AudioDeviceID audioDevice;
    UInt32 size = sizeof(audioDevice);
    const AudioObjectPropertySelector selector = (mode == QAudio::AudioOutput) ? kAudioHardwarePropertyDefaultOutputDevice
                                                                               : kAudioHardwarePropertyDefaultInputDevice;
    AudioObjectPropertyAddress defaultDevicePropertyAddress = { selector,
                                                                kAudioObjectPropertyScopeGlobal,
                                                                kAudioObjectPropertyElementMaster };

    if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                   &defaultDevicePropertyAddress,
                                   0, NULL, &size, &audioDevice) != noErr) {
        qWarning("QAudioDeviceInfo: Unable to find default %s device",  (mode == QAudio::AudioOutput) ? "output" : "input");
        return QByteArray();
    }

    return get_device_info(audioDevice, mode);
#else //iOS
    const auto &devices = (mode == QAudio::AudioOutput) ? CoreAudioSessionManager::instance().outputDevices()
                                                        : CoreAudioSessionManager::instance().inputDevices();
    return !devices.isEmpty() ? devices.first() : QByteArray();
#endif
}

QList<QByteArray> CoreAudioDeviceInfo::availableDevices(QAudio::Mode mode)
{
    QList<QByteArray> devices;
#if defined(Q_OS_OSX)
    UInt32  propSize = 0;
    AudioObjectPropertyAddress audioDevicesPropertyAddress = { kAudioHardwarePropertyDevices,
                                                               kAudioObjectPropertyScopeGlobal,
                                                               kAudioObjectPropertyElementMaster };

    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                       &audioDevicesPropertyAddress,
                                       0, NULL, &propSize) == noErr) {

        const int dc = propSize / sizeof(AudioDeviceID);

        if (dc > 0) {
            AudioDeviceID*  audioDevices = new AudioDeviceID[dc];

            if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &audioDevicesPropertyAddress, 0, NULL, &propSize, audioDevices) == noErr) {
                for (int i = 0; i < dc; ++i) {
                    const QByteArray &info = get_device_info(audioDevices[i], mode);
                    if (!info.isNull())
                        devices << info;
                }
            }

            delete[] audioDevices;
        }
    }
#else //iOS
    if (mode == QAudio::AudioInput) {
        if (CoreAudioSessionManager::instance().category() != CoreAudioSessionManager::PlayAndRecord) {
            CoreAudioSessionManager::instance().setCategory(CoreAudioSessionManager::PlayAndRecord);
        }
    }

    CoreAudioSessionManager::instance().setActive(true);

    if (mode == QAudio::AudioOutput)
        return CoreAudioSessionManager::instance().outputDevices();
    if (mode == QAudio::AudioInput)
        return CoreAudioSessionManager::instance().inputDevices();
#endif

    return devices;
}

QT_END_NAMESPACE

#include "moc_coreaudiodeviceinfo.cpp"
