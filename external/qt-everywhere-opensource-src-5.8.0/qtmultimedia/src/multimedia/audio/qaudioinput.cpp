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


#include "qaudio.h"
#include "qaudiodeviceinfo.h"
#include "qaudiosystem.h"
#include "qaudioinput.h"

#include "qaudiodevicefactory_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QAudioInput
    \brief The QAudioInput class provides an interface for receiving audio data from an audio input device.

    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_audio

    You can construct an audio input with the system's
    \l{QAudioDeviceInfo::defaultInputDevice()}{default audio input
    device}. It is also possible to create QAudioInput with a
    specific QAudioDeviceInfo. When you create the audio input, you
    should also send in the QAudioFormat to be used for the recording
    (see the QAudioFormat class description for details).

    To record to a file:

    QAudioInput lets you record audio with an audio input device. The
    default constructor of this class will use the systems default
    audio device, but you can also specify a QAudioDeviceInfo for a
    specific device. You also need to pass in the QAudioFormat in
    which you wish to record.

    Starting up the QAudioInput is simply a matter of calling start()
    with a QIODevice opened for writing. For instance, to record to a
    file, you can:

    \snippet multimedia-snippets/audio.cpp Audio input class members

    \snippet multimedia-snippets/audio.cpp Audio input setup

    This will start recording if the format specified is supported by
    the input device (you can check this with
    QAudioDeviceInfo::isFormatSupported(). In case there are any
    snags, use the error() function to check what went wrong. We stop
    recording in the \c stopRecording() slot.

    \snippet multimedia-snippets/audio.cpp Audio input stop recording

    At any point in time, QAudioInput will be in one of four states:
    active, suspended, stopped, or idle. These states are specified by
    the QAudio::State enum. You can request a state change directly through
    suspend(), resume(), stop(), reset(), and start(). The current
    state is reported by state(). QAudioOutput will also signal you
    when the state changes (stateChanged()).

    QAudioInput provides several ways of measuring the time that has
    passed since the start() of the recording. The \c processedUSecs()
    function returns the length of the stream in microseconds written,
    i.e., it leaves out the times the audio input was suspended or idle.
    The elapsedUSecs() function returns the time elapsed since start() was called regardless of
    which states the QAudioInput has been in.

    If an error should occur, you can fetch its reason with error().
    The possible error reasons are described by the QAudio::Error
    enum. The QAudioInput will enter the \l{QAudio::}{StoppedState} when
    an error is encountered.  Connect to the stateChanged() signal to
    handle the error:

    \snippet multimedia-snippets/audio.cpp Audio input state changed

    \sa QAudioOutput, QAudioDeviceInfo
*/

/*!
    Construct a new audio input and attach it to \a parent.
    The default audio input device is used with the output
    \a format parameters.
*/

QAudioInput::QAudioInput(const QAudioFormat &format, QObject *parent):
    QObject(parent)
{
    d = QAudioDeviceFactory::createDefaultInputDevice(format);
    connect(d, SIGNAL(notify()), SIGNAL(notify()));
    connect(d, SIGNAL(stateChanged(QAudio::State)), SIGNAL(stateChanged(QAudio::State)));
}

/*!
    Construct a new audio input and attach it to \a parent.
    The device referenced by \a audioDevice is used with the input
    \a format parameters.
*/

QAudioInput::QAudioInput(const QAudioDeviceInfo &audioDevice, const QAudioFormat &format, QObject *parent):
    QObject(parent)
{
    d = QAudioDeviceFactory::createInputDevice(audioDevice, format);
    connect(d, SIGNAL(notify()), SIGNAL(notify()));
    connect(d, SIGNAL(stateChanged(QAudio::State)), SIGNAL(stateChanged(QAudio::State)));
}

/*!
    Destroy this audio input.
*/

QAudioInput::~QAudioInput()
{
    delete d;
}

/*!
    Starts transferring audio data from the system's audio input to the \a device.
    The \a device must have been opened in the \l{QIODevice::WriteOnly}{WriteOnly},
    \l{QIODevice::Append}{Append} or \l{QIODevice::ReadWrite}{ReadWrite} modes.

    If the QAudioInput is able to successfully get audio data, state() returns
    either QAudio::ActiveState or QAudio::IdleState, error() returns QAudio::NoError
    and the stateChanged() signal is emitted.

    If a problem occurs during this process, error() returns QAudio::OpenError,
    state() returns QAudio::StoppedState and the stateChanged() signal is emitted.

    \sa QIODevice
*/

void QAudioInput::start(QIODevice* device)
{
    d->start(device);
}

/*!
    Returns a pointer to the internal QIODevice being used to transfer data from
    the system's audio input. The device will already be open and
    \l{QIODevice::read()}{read()} can read data directly from it.

    \note The pointer will become invalid after the stream is stopped or
    if you start another stream.

    If the QAudioInput is able to access the system's audio device, state() returns
    QAudio::IdleState, error() returns QAudio::NoError
    and the stateChanged() signal is emitted.

    If a problem occurs during this process, error() returns QAudio::OpenError,
    state() returns QAudio::StoppedState and the stateChanged() signal is emitted.

    \sa QIODevice
*/

QIODevice* QAudioInput::start()
{
    return d->start();
}

/*!
    Returns the QAudioFormat being used.
*/

QAudioFormat QAudioInput::format() const
{
    return d->format();
}

/*!
    Stops the audio input, detaching from the system resource.

    Sets error() to QAudio::NoError, state() to QAudio::StoppedState and
    emit stateChanged() signal.
*/

void QAudioInput::stop()
{
    d->stop();
}

/*!
    Drops all audio data in the buffers, resets buffers to zero.
*/

void QAudioInput::reset()
{
    d->reset();
}

/*!
    Stops processing audio data, preserving buffered audio data.

    Sets error() to QAudio::NoError, state() to QAudio::SuspendedState and
    emit stateChanged() signal.
*/

void QAudioInput::suspend()
{
    d->suspend();
}

/*!
    Resumes processing audio data after a suspend().

    Sets error() to QAudio::NoError.
    Sets state() to QAudio::ActiveState if you previously called start(QIODevice*).
    Sets state() to QAudio::IdleState if you previously called start().
    emits stateChanged() signal.
*/

void QAudioInput::resume()
{
     d->resume();
}

/*!
    Sets the audio buffer size to \a value bytes.

    Note: This function can be called anytime before start(), calls to this
    are ignored after start(). It should not be assumed that the buffer size
    set is the actual buffer size used, calling bufferSize() anytime after start()
    will return the actual buffer size being used.

*/

void QAudioInput::setBufferSize(int value)
{
    d->setBufferSize(value);
}

/*!
    Returns the audio buffer size in bytes.

    If called before start(), returns platform default value.
    If called before start() but setBufferSize() was called prior, returns value set by setBufferSize().
    If called after start(), returns the actual buffer size being used. This may not be what was set previously
    by setBufferSize().

*/

int QAudioInput::bufferSize() const
{
    return d->bufferSize();
}

/*!
    Returns the amount of audio data available to read in bytes.

    Note: returned value is only valid while in QAudio::ActiveState or QAudio::IdleState
    state, otherwise returns zero.
*/

int QAudioInput::bytesReady() const
{
    /*
    -If not ActiveState|IdleState, return 0
    -return amount of audio data available to read
    */
    return d->bytesReady();
}

/*!
    Returns the period size in bytes.

    Note: This is the recommended read size in bytes.
*/

int QAudioInput::periodSize() const
{
    return d->periodSize();
}

/*!
    Sets the interval for notify() signal to be emitted.
    This is based on the \a ms of audio data processed
    not on actual real-time.
    The minimum resolution of the timer is platform specific and values
    should be checked with notifyInterval() to confirm actual value
    being used.
*/

void QAudioInput::setNotifyInterval(int ms)
{
    d->setNotifyInterval(ms);
}

/*!
    Returns the notify interval in milliseconds.
*/

int QAudioInput::notifyInterval() const
{
    return d->notifyInterval();
}

/*!
    Sets the input volume to \a volume.

    The volume is scaled linearly from \c 0.0 (silence) to \c 1.0 (full volume). Values outside this
    range will be clamped.

    If the device does not support adjusting the input
    volume then \a volume will be ignored and the input
    volume will remain at 1.0.

    The default volume is \c 1.0.

    Note: Adjustments to the volume will change the volume of this audio stream, not the global volume.
*/
void QAudioInput::setVolume(qreal volume)
{
    qreal v = qBound(qreal(0.0), volume, qreal(1.0));
    d->setVolume(v);
}

/*!
    Returns the input volume.

    If the device does not support adjusting the input volume
    the returned value will be 1.0.
*/
qreal QAudioInput::volume() const
{
    return d->volume();
}

/*!
    Returns the amount of audio data processed since start()
    was called in microseconds.
*/

qint64 QAudioInput::processedUSecs() const
{
    return d->processedUSecs();
}

/*!
    Returns the microseconds since start() was called, including time in Idle and
    Suspend states.
*/

qint64 QAudioInput::elapsedUSecs() const
{
    return d->elapsedUSecs();
}

/*!
    Returns the error state.
*/

QAudio::Error QAudioInput::error() const
{
    return d->error();
}

/*!
    Returns the state of audio processing.
*/

QAudio::State QAudioInput::state() const
{
    return d->state();
}

/*!
    \fn QAudioInput::stateChanged(QAudio::State state)
    This signal is emitted when the device \a state has changed.
*/

/*!
    \fn QAudioInput::notify()
    This signal is emitted when x ms of audio data has been processed
    the interval set by setNotifyInterval(x).
*/

QT_END_NAMESPACE

#include "moc_qaudioinput.cpp"

