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
#include "qaudiooutput.h"

#include "qaudiodevicefactory_p.h"


QT_BEGIN_NAMESPACE

/*!
    \class QAudioOutput
    \brief The QAudioOutput class provides an interface for sending audio data to an audio output device.

    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_audio

    You can construct an audio output with the system's
    \l{QAudioDeviceInfo::defaultOutputDevice()}{default audio output
    device}. It is also possible to create QAudioOutput with a
    specific QAudioDeviceInfo. When you create the audio output, you
    should also send in the QAudioFormat to be used for the playback
    (see the QAudioFormat class description for details).

    To play a file:

    Starting to play an audio stream is simply a matter of calling
    start() with a QIODevice. QAudioOutput will then fetch the data it
    needs from the io device. So playing back an audio file is as
    simple as:

    \snippet multimedia-snippets/audio.cpp Audio output class members

    \snippet multimedia-snippets/audio.cpp Audio output setup

    The file will start playing assuming that the audio system and
    output device support it. If you run out of luck, check what's
    up with the error() function.

    After the file has finished playing, we need to stop the device:

    \snippet multimedia-snippets/audio.cpp Audio output state changed

    At any given time, the QAudioOutput will be in one of four states:
    active, suspended, stopped, or idle. These states are described
    by the QAudio::State enum.
    State changes are reported through the stateChanged() signal. You
    can use this signal to, for instance, update the GUI of the
    application; the mundane example here being changing the state of
    a \c { play/pause } button. You request a state change directly
    with suspend(), stop(), reset(), resume(), and start().

    While the stream is playing, you can set a notify interval in
    milliseconds with setNotifyInterval(). This interval specifies the
    time between two emissions of the notify() signal. This is
    relative to the position in the stream, i.e., if the QAudioOutput
    is in the SuspendedState or the IdleState, the notify() signal is
    not emitted. A typical use-case would be to update a
    \l{QSlider}{slider} that allows seeking in the stream.
    If you want the time since playback started regardless of which
    states the audio output has been in, elapsedUSecs() is the function for you.

    If an error occurs, you can fetch the \l{QAudio::Error}{error
    type} with the error() function. Please see the QAudio::Error enum
    for a description of the possible errors that are reported.  When
    an error is encountered, the state changes to QAudio::StoppedState.
    You can check for errors by connecting to the stateChanged()
    signal:

    \snippet multimedia-snippets/audio.cpp Audio output state changed

    \sa QAudioInput, QAudioDeviceInfo
*/

/*!
    Construct a new audio output and attach it to \a parent.
    The default audio output device is used with the output
    \a format parameters.
*/
QAudioOutput::QAudioOutput(const QAudioFormat &format, QObject *parent):
    QObject(parent)
{
    d = QAudioDeviceFactory::createDefaultOutputDevice(format);
    connect(d, SIGNAL(notify()), SIGNAL(notify()));
    connect(d, SIGNAL(stateChanged(QAudio::State)), SIGNAL(stateChanged(QAudio::State)));
}

/*!
    Construct a new audio output and attach it to \a parent.
    The device referenced by \a audioDevice is used with the output
    \a format parameters.
*/
QAudioOutput::QAudioOutput(const QAudioDeviceInfo &audioDevice, const QAudioFormat &format, QObject *parent):
    QObject(parent)
{
    d = QAudioDeviceFactory::createOutputDevice(audioDevice, format);
    connect(d, SIGNAL(notify()), SIGNAL(notify()));
    connect(d, SIGNAL(stateChanged(QAudio::State)), SIGNAL(stateChanged(QAudio::State)));
}

/*!
    Destroys this audio output.

    This will release any system resources used and free any buffers.
*/
QAudioOutput::~QAudioOutput()
{
    delete d;
}

/*!
    Returns the QAudioFormat being used.

*/
QAudioFormat QAudioOutput::format() const
{
    return d->format();
}

/*!
    Starts transferring audio data from the \a device to the system's audio output.
    The \a device must have been opened in the \l{QIODevice::ReadOnly}{ReadOnly} or
    \l{QIODevice::ReadWrite}{ReadWrite} modes.

    If the QAudioOutput is able to successfully output audio data, state() returns
    QAudio::ActiveState, error() returns QAudio::NoError
    and the stateChanged() signal is emitted.

    If a problem occurs during this process, error() returns QAudio::OpenError,
    state() returns QAudio::StoppedState and the stateChanged() signal is emitted.

    \sa QIODevice
*/
void QAudioOutput::start(QIODevice* device)
{
    d->start(device);
}

/*!
    Returns a pointer to the internal QIODevice being used to transfer data to
    the system's audio output. The device will already be open and
    \l{QIODevice::write()}{write()} can write data directly to it.

    \note The pointer will become invalid after the stream is stopped or
    if you start another stream.

    If the QAudioOutput is able to access the system's audio device, state() returns
    QAudio::IdleState, error() returns QAudio::NoError
    and the stateChanged() signal is emitted.

    If a problem occurs during this process, error() returns QAudio::OpenError,
    state() returns QAudio::StoppedState and the stateChanged() signal is emitted.

    \sa QIODevice
*/
QIODevice* QAudioOutput::start()
{
    return d->start();
}

/*!
    Stops the audio output, detaching from the system resource.

    Sets error() to QAudio::NoError, state() to QAudio::StoppedState and
    emit stateChanged() signal.
*/
void QAudioOutput::stop()
{
    d->stop();
}

/*!
    Drops all audio data in the buffers, resets buffers to zero.

*/
void QAudioOutput::reset()
{
    d->reset();
}

/*!
    Stops processing audio data, preserving buffered audio data.

    Sets error() to QAudio::NoError, state() to QAudio::SuspendedState and
    emits stateChanged() signal.
*/
void QAudioOutput::suspend()
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
void QAudioOutput::resume()
{
     d->resume();
}

/*!
    Returns the number of free bytes available in the audio buffer.

    \note The returned value is only valid while in QAudio::ActiveState or QAudio::IdleState
    state, otherwise returns zero.
*/
int QAudioOutput::bytesFree() const
{
    return d->bytesFree();
}

/*!
    Returns the period size in bytes. This is the amount of data required each period
    to prevent buffer underrun, and to ensure uninterrupted playback.

    \note It is recommended to provide at least enough data for a full period with each
    write operation.
*/
int QAudioOutput::periodSize() const
{
    return d->periodSize();
}

/*!
    Sets the audio buffer size to \a value in bytes.

    \note This function can be called anytime before start().  Calls to this
    are ignored after start(). It should not be assumed that the buffer size
    set is the actual buffer size used - call bufferSize() anytime after start()
    to return the actual buffer size being used.
*/
void QAudioOutput::setBufferSize(int value)
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
int QAudioOutput::bufferSize() const
{
    return d->bufferSize();
}

/*!
    Sets the interval for notify() signal to be emitted.
    This is based on the \a ms of audio data processed,
    not on wall clock time.
    The minimum resolution of the timer is platform specific and values
    should be checked with notifyInterval() to confirm the actual value
    being used.
*/
void QAudioOutput::setNotifyInterval(int ms)
{
    d->setNotifyInterval(ms);
}

/*!
    Returns the notify interval in milliseconds.
*/
int QAudioOutput::notifyInterval() const
{
    return d->notifyInterval();
}

/*!
    Returns the amount of audio data processed since start()
    was called (in microseconds).
*/
qint64 QAudioOutput::processedUSecs() const
{
    return d->processedUSecs();
}

/*!
    Returns the microseconds since start() was called, including time in Idle and
    Suspend states.
*/
qint64 QAudioOutput::elapsedUSecs() const
{
    return d->elapsedUSecs();
}

/*!
    Returns the error state.
*/
QAudio::Error QAudioOutput::error() const
{
    return d->error();
}

/*!
    Returns the state of audio processing.
*/
QAudio::State QAudioOutput::state() const
{
    return d->state();
}

/*!
    Sets the output volume to \a volume.

    The volume is scaled linearly from \c 0.0 (silence) to \c 1.0 (full volume). Values outside this
    range will be clamped.

    The default volume is \c 1.0.

    Note: Adjustments to the volume will change the volume of this audio stream, not the global volume.

    UI volume controls should usually be scaled nonlinearly. For example, using a logarithmic scale
    will produce linear changes in perceived loudness, which is what a user would normally expect
    from a volume control. See QAudio::convertVolume() for more details.
*/
void QAudioOutput::setVolume(qreal volume)
{
    qreal v = qBound(qreal(0.0), volume, qreal(1.0));
    d->setVolume(v);
}

/*!
    Returns the volume between 0.0 and 1.0 inclusive.
*/
qreal QAudioOutput::volume() const
{
    return d->volume();
}

/*!
    Returns the audio category of this audio stream.

    Some platforms can group audio streams into categories
    and manage their volumes independently, or display them
    in a system mixer control.  You can set this property to
    allow the platform to distinguish the purpose of your streams.

    \sa setCategory()
*/
QString QAudioOutput::category() const
{
    return d->category();
}

/*!
    Sets the audio category of this audio stream to \a category.

    Some platforms can group audio streams into categories
    and manage their volumes independently, or display them
    in a system mixer control.  You can set this property to
    allow the platform to distinguish the purpose of your streams.

    Not all platforms support audio stream categorization.  In this
    case, the function call will be ignored.

    Changing an audio output stream's category while it is opened
    will not take effect until it is reopened.
    \sa category()
*/
void QAudioOutput::setCategory(const QString &category)
{
    d->setCategory(category);
}

/*!
    \fn QAudioOutput::stateChanged(QAudio::State state)
    This signal is emitted when the device \a state has changed.
    This is the current state of the audio output.
*/

/*!
    \fn QAudioOutput::notify()
    This signal is emitted when a certain interval of milliseconds
    of audio data has been processed.  The interval is set by
    setNotifyInterval().
*/

QT_END_NAMESPACE

#include "moc_qaudiooutput.cpp"
