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

#include "qaudiosystem.h"

QT_BEGIN_NAMESPACE

/*!
    \class QAbstractAudioDeviceInfo
    \brief The QAbstractAudioDeviceInfo class is a base class for audio backends.

    \ingroup multimedia
    \ingroup multimedia_audio
    \inmodule QtMultimedia

    This class implements the audio functionality for
    QAudioDeviceInfo, i.e., QAudioDeviceInfo keeps a
    QAbstractAudioDeviceInfo and routes function calls to it. For a
    description of the functionality that QAbstractAudioDeviceInfo
    implements, you can read the class and functions documentation of
    QAudioDeviceInfo.

    \sa QAudioDeviceInfo
    \sa QAbstractAudioOutput, QAbstractAudioInput
*/

/*!
    \fn virtual QAudioFormat QAbstractAudioDeviceInfo::preferredFormat() const
    Returns the recommended settings to use.
*/

/*!
    \fn virtual bool QAbstractAudioDeviceInfo::isFormatSupported(const QAudioFormat& format) const
    Returns true if \a format is available from audio device.
*/

/*!
    \fn virtual QString QAbstractAudioDeviceInfo::deviceName() const
    Returns the audio device name.
*/

/*!
    \fn virtual QStringList QAbstractAudioDeviceInfo::supportedCodecs()
    Returns the list of currently available codecs.
*/

/*!
    \fn virtual QList<int> QAbstractAudioDeviceInfo::supportedSampleRates()
    Returns the list of currently available sample rates.
*/

/*!
    \fn virtual QList<int> QAbstractAudioDeviceInfo::supportedChannelCounts()
    Returns the list of currently available channels.
*/

/*!
    \fn virtual QList<int> QAbstractAudioDeviceInfo::supportedSampleSizes()
    Returns the list of currently available sample sizes.
*/

/*!
    \fn virtual QList<QAudioFormat::Endian> QAbstractAudioDeviceInfo::supportedByteOrders()
    Returns the list of currently available byte orders.
*/

/*!
    \fn virtual QList<QAudioFormat::SampleType> QAbstractAudioDeviceInfo::supportedSampleTypes()
    Returns the list of currently available sample types.
*/

/*!
    \class QAbstractAudioOutput
    \brief The QAbstractAudioOutput class is a base class for audio backends.

    \ingroup multimedia
    \inmodule QtMultimedia

    QAbstractAudioOutput implements audio functionality for
    QAudioOutput, i.e., QAudioOutput routes function calls to
    QAbstractAudioOutput. For a description of the functionality that
    is implemented, see the QAudioOutput class and function
    descriptions.

    \sa QAudioOutput
*/

/*!
    \fn virtual void QAbstractAudioOutput::start(QIODevice* device)
    Uses the \a device as the QIODevice to transfer data.
*/

/*!
    \fn virtual QIODevice* QAbstractAudioOutput::start()
    Returns a pointer to the QIODevice being used to handle
    the data transfer. This QIODevice can be used to write() audio data directly.
*/

/*!
    \fn virtual void QAbstractAudioOutput::stop()
    Stops the audio output.
*/

/*!
    \fn virtual void QAbstractAudioOutput::reset()
    Drops all audio data in the buffers, resets buffers to zero.
*/

/*!
    \fn virtual void QAbstractAudioOutput::suspend()
    Stops processing audio data, preserving buffered audio data.
*/

/*!
    \fn virtual void QAbstractAudioOutput::resume()
    Resumes processing audio data after a suspend()
*/

/*!
    \fn virtual int QAbstractAudioOutput::bytesFree() const
    Returns the free space available in bytes in the audio buffer.
*/

/*!
    \fn virtual int QAbstractAudioOutput::periodSize() const
    Returns the period size in bytes.
*/

/*!
    \fn virtual void QAbstractAudioOutput::setBufferSize(int value)
    Sets the audio buffer size to \a value in bytes.
*/

/*!
    \fn virtual int QAbstractAudioOutput::bufferSize() const
    Returns the audio buffer size in bytes.
*/

/*!
    \fn virtual void QAbstractAudioOutput::setNotifyInterval(int ms)
    Sets the interval for notify() signal to be emitted. This is based on the \a ms
    of audio data processed not on actual real-time. The resolution of the timer
    is platform specific.
*/

/*!
    \fn virtual int QAbstractAudioOutput::notifyInterval() const
    Returns the notify interval in milliseconds.
*/

/*!
    \fn virtual qint64 QAbstractAudioOutput::processedUSecs() const
    Returns the amount of audio data processed since start() was called in milliseconds.
*/

/*!
    \fn virtual qint64 QAbstractAudioOutput::elapsedUSecs() const
    Returns the milliseconds since start() was called, including time in Idle and suspend states.
*/

/*!
    \fn virtual QAudio::Error QAbstractAudioOutput::error() const
    Returns the error state.
*/

/*!
    \fn virtual QAudio::State QAbstractAudioOutput::state() const
    Returns the state of audio processing.
*/

/*!
    \fn virtual void QAbstractAudioOutput::setFormat(const QAudioFormat& fmt)
    Set the QAudioFormat to use to \a fmt.
    Setting the format is only allowable while in QAudio::StoppedState.
*/

/*!
    \fn virtual QAudioFormat QAbstractAudioOutput::format() const
    Returns the QAudioFormat being used.
*/

/*!
    \fn virtual void QAbstractAudioOutput::setVolume(qreal volume)
    Sets the volume.
    Where \a volume is between 0.0 and 1.0.
*/

/*!
    \fn virtual qreal QAbstractAudioOutput::volume() const
    Returns the volume in the range 0.0 and 1.0.
*/

/*!
    \fn QAbstractAudioOutput::errorChanged(QAudio::Error error)
    This signal is emitted when the \a error state has changed.
*/

/*!
    \fn QAbstractAudioOutput::stateChanged(QAudio::State state)
    This signal is emitted when the device \a state has changed.
*/

/*!
    \fn QAbstractAudioOutput::notify()
    This signal is emitted when x ms of audio data has been processed
    the interval set by setNotifyInterval(x).
*/


/*!
    \class QAbstractAudioInput
    \brief The QAbstractAudioInput class provides access for QAudioInput to access the audio
    device provided by the plugin.

    \ingroup multimedia
    \inmodule QtMultimedia

    QAudioDeviceInput keeps an instance of QAbstractAudioInput and
    routes calls to functions of the same name to QAbstractAudioInput.
    This means that it is QAbstractAudioInput that implements the
    audio functionality. For a description of the functionality, see
    the QAudioInput class description.

    \sa QAudioInput
*/

/*!
    \fn virtual void QAbstractAudioInput::start(QIODevice* device)
    Uses the \a device as the QIODevice to transfer data.
*/

/*!
    \fn virtual QIODevice* QAbstractAudioInput::start()
    Returns a pointer to the QIODevice being used to handle
    the data transfer. This QIODevice can be used to read() audio data directly.
*/

/*!
    \fn virtual void QAbstractAudioInput::stop()
    Stops the audio input.
*/

/*!
    \fn virtual void QAbstractAudioInput::reset()
    Drops all audio data in the buffers, resets buffers to zero.
*/

/*!
    \fn virtual void QAbstractAudioInput::suspend()
    Stops processing audio data, preserving buffered audio data.
*/

/*!
    \fn virtual void QAbstractAudioInput::resume()
    Resumes processing audio data after a suspend().
*/

/*!
    \fn virtual int QAbstractAudioInput::bytesReady() const
    Returns the amount of audio data available to read in bytes.
*/

/*!
    \fn virtual int QAbstractAudioInput::periodSize() const
    Returns the period size in bytes.
*/

/*!
    \fn virtual void QAbstractAudioInput::setBufferSize(int value)
    Sets the audio buffer size to \a value in milliseconds.
*/

/*!
    \fn virtual int QAbstractAudioInput::bufferSize() const
    Returns the audio buffer size in milliseconds.
*/

/*!
    \fn virtual void QAbstractAudioInput::setNotifyInterval(int ms)
    Sets the interval for notify() signal to be emitted. This is based
    on the \a ms of audio data processed not on actual real-time.
    The resolution of the timer is platform specific.
*/

/*!
    \fn virtual int QAbstractAudioInput::notifyInterval() const
    Returns the notify interval in milliseconds.
*/

/*!
    \fn virtual qint64 QAbstractAudioInput::processedUSecs() const
    Returns the amount of audio data processed since start() was called in milliseconds.
*/

/*!
    \fn virtual qint64 QAbstractAudioInput::elapsedUSecs() const
    Returns the milliseconds since start() was called, including time in Idle and suspend states.
*/

/*!
    \fn virtual QAudio::Error QAbstractAudioInput::error() const
    Returns the error state.
*/

/*!
    \fn virtual QAudio::State QAbstractAudioInput::state() const
    Returns the state of audio processing.
*/

/*!
    \fn virtual void QAbstractAudioInput::setFormat(const QAudioFormat& fmt)
    Set the QAudioFormat to use to \a fmt.
    Setting the format is only allowable while in QAudio::StoppedState.
*/

/*!
    \fn virtual QAudioFormat QAbstractAudioInput::format() const
    Returns the QAudioFormat being used
*/

/*!
    \fn QAbstractAudioInput::errorChanged(QAudio::Error error)
    This signal is emitted when the \a error state has changed.
*/

/*!
    \fn QAbstractAudioInput::stateChanged(QAudio::State state)
    This signal is emitted when the device \a state has changed.
*/

/*!
    \fn QAbstractAudioInput::notify()
    This signal is emitted when x ms of audio data has been processed
    the interval set by setNotifyInterval(x).
*/


QT_END_NAMESPACE

#include "moc_qaudiosystem.cpp"
