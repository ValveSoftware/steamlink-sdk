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

#include "qaudiodevicefactory_p.h"
#include "qaudiosystem.h"
#include "qaudiodeviceinfo.h"

#include <QtCore/qmap.h>

QT_BEGIN_NAMESPACE

static void qRegisterAudioDeviceInfoMetaTypes()
{
    qRegisterMetaType<QAudioDeviceInfo>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterAudioDeviceInfoMetaTypes)

class QAudioDeviceInfoPrivate : public QSharedData
{
public:
    QAudioDeviceInfoPrivate()
        : mode(QAudio::AudioOutput)
        , info(0)
    {
    }

    QAudioDeviceInfoPrivate(const QString &r, const QByteArray &h, QAudio::Mode m):
        realm(r), handle(h), mode(m)
    {
        if (!handle.isEmpty())
            info = QAudioDeviceFactory::audioDeviceInfo(realm, handle, mode);
        else
            info = NULL;
    }

    QAudioDeviceInfoPrivate(const QAudioDeviceInfoPrivate &other):
        QSharedData(other),
        realm(other.realm), handle(other.handle), mode(other.mode)
    {
        info = QAudioDeviceFactory::audioDeviceInfo(realm, handle, mode);
    }

    QAudioDeviceInfoPrivate& operator=(const QAudioDeviceInfoPrivate &other)
    {
        delete info;

        realm = other.realm;
        handle = other.handle;
        mode = other.mode;
        info = QAudioDeviceFactory::audioDeviceInfo(realm, handle, mode);
        return *this;
    }

    ~QAudioDeviceInfoPrivate()
    {
        delete info;
    }

    QString     realm;
    QByteArray  handle;
    QAudio::Mode mode;
    QAbstractAudioDeviceInfo*   info;
};


/*!
    \class QAudioDeviceInfo
    \brief The QAudioDeviceInfo class provides an interface to query audio devices and their functionality.
    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_audio

    QAudioDeviceInfo lets you query for audio devices--such as sound
    cards and USB headsets--that are currently available on the system.
    The audio devices available are dependent on the platform or audio plugins installed.

    A QAudioDeviceInfo is used by Qt to construct
    classes that communicate with the device--such as
    QAudioInput, and QAudioOutput.

    You can also query each device for the formats it supports. A
    format in this context is a set consisting of a specific byte
    order, channel, codec, frequency, sample rate, and sample type.  A
    format is represented by the QAudioFormat class.

    The values supported by the device for each of these
    parameters can be fetched with
    supportedByteOrders(), supportedChannelCounts(), supportedCodecs(),
    supportedSampleRates(), supportedSampleSizes(), and
    supportedSampleTypes(). The combinations supported are dependent on the platform,
    audio plugins installed and the audio device capabilities. If you need a
    specific format, you can check if
    the device supports it with isFormatSupported(), or fetch a
    supported format that is as close as possible to the format with
    nearestFormat(). For instance:

    \snippet multimedia-snippets/audio.cpp Setting audio format

    The static
    functions defaultInputDevice(), defaultOutputDevice(), and
    availableDevices() let you get a list of all available
    devices. Devices are fetched according to the value of mode
    this is specified by the \l {QAudio}::Mode enum.
    The QAudioDeviceInfo returned are only valid for the \l {QAudio}::Mode.

    For instance:

    \snippet multimedia-snippets/audio.cpp Dumping audio formats

    In this code sample, we loop through all devices that are able to output
    sound, i.e., play an audio stream in a supported format. For each device we
    find, we simply print the deviceName().

    \sa QAudioOutput, QAudioInput
*/

/*!
    Constructs an empty QAudioDeviceInfo object.
*/
QAudioDeviceInfo::QAudioDeviceInfo():
    d(new QAudioDeviceInfoPrivate)
{
}

/*!
    Constructs a copy of \a other.
*/
QAudioDeviceInfo::QAudioDeviceInfo(const QAudioDeviceInfo& other):
    d(other.d)
{
}

/*!
    Destroy this audio device info.
*/
QAudioDeviceInfo::~QAudioDeviceInfo()
{
}

/*!
    Sets the QAudioDeviceInfo object to be equal to \a other.
*/
QAudioDeviceInfo& QAudioDeviceInfo::operator=(const QAudioDeviceInfo &other)
{
    d = other.d;
    return *this;
}

/*!
    Returns true if this QAudioDeviceInfo class represents the
    same audio device as \a other.
*/
bool QAudioDeviceInfo::operator ==(const QAudioDeviceInfo &other) const
{
    if (d == other.d)
        return true;
    if (d->realm == other.d->realm
            && d->mode == other.d->mode
            && d->handle == other.d->handle
            && deviceName() == other.deviceName())
        return true;
    return false;
}

/*!
    Returns true if this QAudioDeviceInfo class represents a
    different audio device than \a other
*/
bool QAudioDeviceInfo::operator !=(const QAudioDeviceInfo &other) const
{
    return !operator==(other);
}

/*!
    Returns whether this QAudioDeviceInfo object holds a valid device definition.
*/
bool QAudioDeviceInfo::isNull() const
{
    return d->info == 0;
}

/*!
    Returns the human readable name of the audio device.

    Device names vary depending on the platform/audio plugin being used.

    They are a unique string identifier for the audio device.

    eg. default, Intel, U0x46d0x9a4
*/
QString QAudioDeviceInfo::deviceName() const
{
    return isNull() ? QString() : d->info->deviceName();
}

/*!
    Returns true if the supplied \a settings are supported by the audio
    device described by this QAudioDeviceInfo.
*/
bool QAudioDeviceInfo::isFormatSupported(const QAudioFormat &settings) const
{
    return isNull() ? false : d->info->isFormatSupported(settings);
}

/*!
    Returns the default audio format settings for this device.

    These settings are provided by the platform/audio plugin being used.

    They are also dependent on the \l {QAudio}::Mode being used.

    A typical audio system would provide something like:
    \list
    \li Input settings: 8000Hz mono 8 bit.
    \li Output settings: 44100Hz stereo 16 bit little endian.
    \endlist
*/
QAudioFormat QAudioDeviceInfo::preferredFormat() const
{
    return isNull() ? QAudioFormat() : d->info->preferredFormat();
}

/*!
    Returns the closest QAudioFormat to the supplied \a settings that the system supports.

    These settings are provided by the platform/audio plugin being used.

    They are also dependent on the \l {QAudio}::Mode being used.
*/
QAudioFormat QAudioDeviceInfo::nearestFormat(const QAudioFormat &settings) const
{
    if (isFormatSupported(settings))
        return settings;

    QAudioFormat nearest = settings;

    QList<QString> testCodecs = supportedCodecs();
    QList<int> testChannels = supportedChannelCounts();
    QList<QAudioFormat::Endian> testByteOrders = supportedByteOrders();
    QList<QAudioFormat::SampleType> testSampleTypes;
    QList<QAudioFormat::SampleType> sampleTypesAvailable = supportedSampleTypes();
    QMap<int,int> testSampleRates;
    QList<int> sampleRatesAvailable = supportedSampleRates();
    QMap<int,int> testSampleSizes;
    QList<int> sampleSizesAvailable = supportedSampleSizes();

    // Get sorted lists for checking
    if (testCodecs.contains(settings.codec())) {
        testCodecs.removeAll(settings.codec());
        testCodecs.insert(0, settings.codec());
    }
    testChannels.removeAll(settings.channelCount());
    testChannels.insert(0, settings.channelCount());
    testByteOrders.removeAll(settings.byteOrder());
    testByteOrders.insert(0, settings.byteOrder());

    if (sampleTypesAvailable.contains(settings.sampleType()))
        testSampleTypes.append(settings.sampleType());
    if (sampleTypesAvailable.contains(QAudioFormat::SignedInt))
        testSampleTypes.append(QAudioFormat::SignedInt);
    if (sampleTypesAvailable.contains(QAudioFormat::UnSignedInt))
        testSampleTypes.append(QAudioFormat::UnSignedInt);
    if (sampleTypesAvailable.contains(QAudioFormat::Float))
        testSampleTypes.append(QAudioFormat::Float);

    if (sampleSizesAvailable.contains(settings.sampleSize()))
        testSampleSizes.insert(0,settings.sampleSize());
    sampleSizesAvailable.removeAll(settings.sampleSize());
    for (int size : qAsConst(sampleSizesAvailable)) {
        int larger  = (size > settings.sampleSize()) ? size : settings.sampleSize();
        int smaller = (size > settings.sampleSize()) ? settings.sampleSize() : size;
        bool isMultiple = ( 0 == (larger % smaller));
        int diff = larger - smaller;
        testSampleSizes.insert((isMultiple ? diff : diff+100000), size);
    }
    if (sampleRatesAvailable.contains(settings.sampleRate()))
        testSampleRates.insert(0,settings.sampleRate());
    sampleRatesAvailable.removeAll(settings.sampleRate());
    for (int sampleRate : qAsConst(sampleRatesAvailable)) {
        int larger  = (sampleRate > settings.sampleRate()) ? sampleRate : settings.sampleRate();
        int smaller = (sampleRate > settings.sampleRate()) ? settings.sampleRate() : sampleRate;
        bool isMultiple = ( 0 == (larger % smaller));
        int diff = larger - smaller;
        testSampleRates.insert((isMultiple ? diff : diff+100000), sampleRate);
    }

    // Try to find nearest
    for (const QString &codec : qAsConst(testCodecs)) {
        nearest.setCodec(codec);
        for (QAudioFormat::Endian order : qAsConst(testByteOrders)) {
            nearest.setByteOrder(order);
            for (QAudioFormat::SampleType sample : qAsConst(testSampleTypes)) {
                nearest.setSampleType(sample);
                QMapIterator<int, int> sz(testSampleSizes);
                while (sz.hasNext()) {
                    sz.next();
                    nearest.setSampleSize(sz.value());
                    for (int channel : qAsConst(testChannels)) {
                        nearest.setChannelCount(channel);
                        QMapIterator<int, int> i(testSampleRates);
                        while (i.hasNext()) {
                            i.next();
                            nearest.setSampleRate(i.value());
                            if (isFormatSupported(nearest))
                                return nearest;
                        }
                    }
                }
            }
        }
    }
    //Fallback
    return preferredFormat();
}

/*!
    Returns a list of supported codecs.

    All platform and plugin implementations should provide support for:

    "audio/pcm" - Linear PCM

    For writing plugins to support additional codecs refer to:

    http://www.iana.org/assignments/media-types/audio/
*/
QStringList QAudioDeviceInfo::supportedCodecs() const
{
    return isNull() ? QStringList() : d->info->supportedCodecs();
}

/*!
    Returns a list of supported sample rates (in Hertz).

*/
QList<int> QAudioDeviceInfo::supportedSampleRates() const
{
    return isNull() ? QList<int>() : d->info->supportedSampleRates();
}

/*!
    Returns a list of supported channel counts.

    This is typically 1 for mono sound, or 2 for stereo sound.

*/
QList<int> QAudioDeviceInfo::supportedChannelCounts() const
{
    return isNull() ? QList<int>() : d->info->supportedChannelCounts();
}

/*!
    Returns a list of supported sample sizes (in bits).

    Typically this will include 8 and 16 bit sample sizes.

*/
QList<int> QAudioDeviceInfo::supportedSampleSizes() const
{
    return isNull() ? QList<int>() : d->info->supportedSampleSizes();
}

/*!
    Returns a list of supported byte orders.
*/
QList<QAudioFormat::Endian> QAudioDeviceInfo::supportedByteOrders() const
{
    return isNull() ? QList<QAudioFormat::Endian>() : d->info->supportedByteOrders();
}

/*!
    Returns a list of supported sample types.
*/
QList<QAudioFormat::SampleType> QAudioDeviceInfo::supportedSampleTypes() const
{
    return isNull() ? QList<QAudioFormat::SampleType>() : d->info->supportedSampleTypes();
}

/*!
    Returns the information for the default input audio device.
    All platform and audio plugin implementations provide a default audio device to use.
*/
QAudioDeviceInfo QAudioDeviceInfo::defaultInputDevice()
{
    return QAudioDeviceFactory::defaultDevice(QAudio::AudioInput);
}

/*!
    Returns the information for the default output audio device.
    All platform and audio plugin implementations provide a default audio device to use.
*/
QAudioDeviceInfo QAudioDeviceInfo::defaultOutputDevice()
{
    return QAudioDeviceFactory::defaultDevice(QAudio::AudioOutput);
}

/*!
    Returns a list of audio devices that support \a mode.
*/
QList<QAudioDeviceInfo> QAudioDeviceInfo::availableDevices(QAudio::Mode mode)
{
    return QAudioDeviceFactory::availableDevices(mode);
}


/*!
    \internal
*/
QAudioDeviceInfo::QAudioDeviceInfo(const QString &realm, const QByteArray &handle, QAudio::Mode mode):
    d(new QAudioDeviceInfoPrivate(realm, handle, mode))
{
}

/*!
    \internal
*/
QString QAudioDeviceInfo::realm() const
{
    return d->realm;
}

/*!
    \internal
*/
QByteArray QAudioDeviceInfo::handle() const
{
    return d->handle;
}


/*!
    \internal
*/
QAudio::Mode QAudioDeviceInfo::mode() const
{
    return d->mode;
}

QT_END_NAMESPACE

