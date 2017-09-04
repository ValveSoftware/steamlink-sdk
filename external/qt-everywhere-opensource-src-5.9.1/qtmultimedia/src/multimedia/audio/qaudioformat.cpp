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
#include <QDebug>
#include <qaudioformat.h>


QT_BEGIN_NAMESPACE

static void qRegisterAudioFormatMetaTypes()
{
    qRegisterMetaType<QAudioFormat>();
    qRegisterMetaType<QAudioFormat::SampleType>();
    qRegisterMetaType<QAudioFormat::Endian>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterAudioFormatMetaTypes)

class QAudioFormatPrivate : public QSharedData
{
public:
    QAudioFormatPrivate()
    {
        sampleRate = -1;
        channels = -1;
        sampleSize = -1;
        byteOrder = QAudioFormat::Endian(QSysInfo::ByteOrder);
        sampleType = QAudioFormat::Unknown;
    }

    QAudioFormatPrivate(const QAudioFormatPrivate &other):
        QSharedData(other),
        codec(other.codec),
        byteOrder(other.byteOrder),
        sampleType(other.sampleType),
        sampleRate(other.sampleRate),
        channels(other.channels),
        sampleSize(other.sampleSize)
    {
    }

    QAudioFormatPrivate& operator=(const QAudioFormatPrivate &other)
    {
        codec = other.codec;
        byteOrder = other.byteOrder;
        sampleType = other.sampleType;
        sampleRate = other.sampleRate;
        channels = other.channels;
        sampleSize = other.sampleSize;

        return *this;
    }

    QString codec;
    QAudioFormat::Endian byteOrder;
    QAudioFormat::SampleType sampleType;
    int sampleRate;
    int channels;
    int sampleSize;
};

/*!
    \class QAudioFormat
    \brief The QAudioFormat class stores audio stream parameter information.

    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_audio

    An audio format specifies how data in an audio stream is arranged,
    i.e, how the stream is to be interpreted. The encoding itself is
    specified by the codec() used for the stream.

    In addition to the encoding, QAudioFormat contains other
    parameters that further specify how the audio sample data is arranged.
    These are the frequency, the number of channels, the sample size,
    the sample type, and the byte order. The following table describes
    these in more detail.

    \table
        \header
            \li Parameter
            \li Description
        \row
            \li Sample Rate
            \li Samples per second of audio data in Hertz.
        \row
            \li Number of channels
            \li The number of audio channels (typically one for mono
               or two for stereo)
        \row
            \li Sample size
            \li How much data is stored in each sample (typically 8
               or 16 bits)
        \row
            \li Sample type
            \li Numerical representation of sample (typically signed integer,
               unsigned integer or float)
        \row
            \li Byte order
            \li Byte ordering of sample (typically little endian, big endian)
    \endtable

    This class is typically used in conjunction with QAudioInput or
    QAudioOutput to allow you to specify the parameters of the audio
    stream being read or written, or with QAudioBuffer when dealing with
    samples in memory.

    You can obtain audio formats compatible with the audio device used
    through functions in QAudioDeviceInfo. This class also lets you
    query available parameter values for a device, so that you can set
    the parameters yourself. See the \l QAudioDeviceInfo class
    description for details. You need to know the format of the audio
    streams you wish to play or record.

    In the common case of interleaved linear PCM data, the codec will
    be "audio/pcm", and the samples for all channels will be interleaved.
    One sample for each channel for the same instant in time is referred
    to as a frame in Qt Multimedia (and other places).
*/

/*!
    Construct a new audio format.

    Values are initialized as follows:
    \list
    \li sampleRate()  = -1
    \li channelCount() = -1
    \li sampleSize() = -1
    \li byteOrder()  = QAudioFormat::Endian(QSysInfo::ByteOrder)
    \li sampleType() = QAudioFormat::Unknown
    \c codec()      = ""
    \endlist
*/
QAudioFormat::QAudioFormat():
    d(new QAudioFormatPrivate)
{
}

/*!
    Construct a new audio format using \a other.
*/
QAudioFormat::QAudioFormat(const QAudioFormat &other):
    d(other.d)
{
}

/*!
    Destroy this audio format.
*/
QAudioFormat::~QAudioFormat()
{
}

/*!
    Assigns \a other to this QAudioFormat implementation.
*/
QAudioFormat& QAudioFormat::operator=(const QAudioFormat &other)
{
    d = other.d;
    return *this;
}

/*!
  Returns true if this QAudioFormat is equal to the \a other
  QAudioFormat; otherwise returns false.

  All elements of QAudioFormat are used for the comparison.
*/
bool QAudioFormat::operator==(const QAudioFormat &other) const
{
    return d->sampleRate == other.d->sampleRate &&
            d->channels == other.d->channels &&
            d->sampleSize == other.d->sampleSize &&
            d->byteOrder == other.d->byteOrder &&
            d->codec == other.d->codec &&
            d->sampleType == other.d->sampleType;
}

/*!
  Returns true if this QAudioFormat is not equal to the \a other
  QAudioFormat; otherwise returns false.

  All elements of QAudioFormat are used for the comparison.
*/
bool QAudioFormat::operator!=(const QAudioFormat& other) const
{
    return !(*this == other);
}

/*!
    Returns true if all of the parameters are valid.
*/
bool QAudioFormat::isValid() const
{
    return d->sampleRate != -1 && d->channels != -1 && d->sampleSize != -1 &&
            d->sampleType != QAudioFormat::Unknown && !d->codec.isEmpty();
}

/*!
   Sets the sample rate to \a samplerate Hertz.

*/
void QAudioFormat::setSampleRate(int samplerate)
{
    d->sampleRate = samplerate;
}

/*!
    Returns the current sample rate in Hertz.

*/
int QAudioFormat::sampleRate() const
{
    return d->sampleRate;
}

/*!
   Sets the channel count to \a channels.

*/
void QAudioFormat::setChannelCount(int channels)
{
    d->channels = channels;
}

/*!
    Returns the current channel count value.

*/
int QAudioFormat::channelCount() const
{
    return d->channels;
}

/*!
   Sets the sample size to the \a sampleSize specified, in bits.

   This is typically 8 or 16, but some systems may support higher sample sizes.
*/
void QAudioFormat::setSampleSize(int sampleSize)
{
    d->sampleSize = sampleSize;
}

/*!
    Returns the current sample size value, in bits.

    \sa bytesPerFrame()
*/
int QAudioFormat::sampleSize() const
{
    return d->sampleSize;
}

/*!
   Sets the codec to \a codec.

   The parameter to this function should be one of the types
   reported by the QAudioDeviceInfo::supportedCodecs() function
   for the audio device you are working with.

   \sa QAudioDeviceInfo::supportedCodecs()
*/
void QAudioFormat::setCodec(const QString &codec)
{
    d->codec = codec;
}

/*!
    Returns the current codec identifier.

   \sa QAudioDeviceInfo::supportedCodecs()
*/
QString QAudioFormat::codec() const
{
    return d->codec;
}

/*!
   Sets the byteOrder to \a byteOrder.
*/
void QAudioFormat::setByteOrder(QAudioFormat::Endian byteOrder)
{
    d->byteOrder = byteOrder;
}

/*!
    Returns the current byteOrder value.
*/
QAudioFormat::Endian QAudioFormat::byteOrder() const
{
    return d->byteOrder;
}

/*!
   Sets the sampleType to \a sampleType.
*/
void QAudioFormat::setSampleType(QAudioFormat::SampleType sampleType)
{
    d->sampleType = sampleType;
}

/*!
    Returns the current SampleType value.
*/
QAudioFormat::SampleType QAudioFormat::sampleType() const
{
    return d->sampleType;
}

/*!
    Returns the number of bytes required for this audio format for \a duration microseconds.

    Returns 0 if this format is not valid.

    Note that some rounding may occur if \a duration is not an exact fraction of the
    sampleRate().

    \sa durationForBytes()
 */
qint32 QAudioFormat::bytesForDuration(qint64 duration) const
{
    return bytesPerFrame() * framesForDuration(duration);
}

/*!
    Returns the number of microseconds represented by \a bytes in this format.

    Returns 0 if this format is not valid.

    Note that some rounding may occur if \a bytes is not an exact multiple
    of the number of bytes per frame.

    \sa bytesForDuration()
*/
qint64 QAudioFormat::durationForBytes(qint32 bytes) const
{
    if (!isValid() || bytes <= 0)
        return 0;

    // We round the byte count to ensure whole frames
    return qint64(1000000LL * (bytes / bytesPerFrame())) / sampleRate();
}

/*!
    Returns the number of bytes required for \a frameCount frames of this format.

    Returns 0 if this format is not valid.

    \sa bytesForDuration()
*/
qint32 QAudioFormat::bytesForFrames(qint32 frameCount) const
{
    return frameCount * bytesPerFrame();
}

/*!
    Returns the number of frames represented by \a byteCount in this format.

    Note that some rounding may occur if \a byteCount is not an exact multiple
    of the number of bytes per frame.

    Each frame has one sample per channel.

    \sa framesForDuration()
*/
qint32 QAudioFormat::framesForBytes(qint32 byteCount) const
{
    int size = bytesPerFrame();
    if (size > 0)
        return byteCount / size;
    return 0;
}

/*!
    Returns the number of frames required to represent \a duration microseconds in this format.

    Note that some rounding may occur if \a duration is not an exact fraction of the
    \l sampleRate().
*/
qint32 QAudioFormat::framesForDuration(qint64 duration) const
{
    if (!isValid())
        return 0;

    return qint32((duration * sampleRate()) / 1000000LL);
}

/*!
    Return the number of microseconds represented by \a frameCount frames in this format.
*/
qint64 QAudioFormat::durationForFrames(qint32 frameCount) const
{
    if (!isValid() || frameCount <= 0)
        return 0;

    return (frameCount * 1000000LL) / sampleRate();
}

/*!
    Returns the number of bytes required to represent one frame (a sample in each channel) in this format.

    Returns 0 if this format is invalid.
*/
int QAudioFormat::bytesPerFrame() const
{
    if (!isValid())
        return 0;

    return (sampleSize() * channelCount()) / 8;
}

/*!
    \enum QAudioFormat::SampleType

    \value Unknown       Not Set
    \value SignedInt     Samples are signed integers
    \value UnSignedInt   Samples are unsigned intergers
    \value Float         Samples are floats
*/

/*!
    \enum QAudioFormat::Endian

    \value BigEndian     Samples are big endian byte order
    \value LittleEndian  Samples are little endian byte order
*/

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, QAudioFormat::Endian endian)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (endian) {
        case QAudioFormat::BigEndian:
            dbg << "BigEndian";
            break;
        case QAudioFormat::LittleEndian:
            dbg << "LittleEndian";
            break;
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, QAudioFormat::SampleType type)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (type) {
        case QAudioFormat::SignedInt:
            dbg << "SignedInt";
            break;
        case QAudioFormat::UnSignedInt:
            dbg << "UnSignedInt";
            break;
        case QAudioFormat::Float:
            dbg << "Float";
            break;
       default:
            dbg << "Unknown";
            break;
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, const QAudioFormat &f)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "QAudioFormat(" << f.sampleRate() << "Hz, "
        << f.sampleSize() << "bit, channelCount=" << f.channelCount()
        << ", sampleType=" << f.sampleType() << ", byteOrder=" << f.byteOrder()
        << ", codec=" << f.codec() << ')';

    return dbg;
}
#endif



QT_END_NAMESPACE

