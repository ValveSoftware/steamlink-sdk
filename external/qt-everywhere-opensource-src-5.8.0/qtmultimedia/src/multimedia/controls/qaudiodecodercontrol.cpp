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

#include "qmediacontrol_p.h"
#include "qaudiodecodercontrol.h"

QT_BEGIN_NAMESPACE


/*!
    \class QAudioDecoderControl
    \inmodule QtMultimedia


    \ingroup multimedia_control

    \brief The QAudioDecoderControl class provides access to the audio decoding
    functionality of a QMediaService.

    \preliminary

    The functionality provided by this control is exposed to application
    code through the QAudioDecoder class.

    The interface name of QAudioDecoderControl is \c org.qt-project.qt.audiodecodercontrol/5.0 as
    defined in QAudioDecoderControl_iid.

    \sa QMediaService::requestControl(), QAudioDecoder
*/

/*!
    \macro QAudioDecoderControl_iid

    \c org.qt-project.qt.audiodecodercontrol/5.0

    Defines the interface name of the QAudioDecoderControl class.

    \relates QAudioDecoderControl
*/

/*!
    Destroys an audio decoder control.
*/
QAudioDecoderControl::~QAudioDecoderControl()
{
}

/*!
    Constructs a new audio decoder control with the given \a parent.
*/
QAudioDecoderControl::QAudioDecoderControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    \fn QAudioDecoderControl::state() const

    Returns the state of a player control.
*/

/*!
    \fn QAudioDecoderControl::stateChanged(QAudioDecoder::State state)

    Signals that the \a state of a player control has changed.

    \sa state()
*/

/*!
    \fn QAudioDecoderControl::sourceFilename() const

    Returns the current media source filename, or a null QString if none (or a device)
*/

/*!
    \fn QAudioDecoderControl::setSourceFilename(const QString &fileName)

    Sets the current source to \a fileName.  Changing the source will
    stop any current decoding and discard any buffers.

    Sources are exclusive, so only one can be set.
*/

/*!
    \fn QAudioDecoderControl::sourceDevice() const

    Returns the current media source QIODevice, or 0 if none (or a file).
*/

/*!
    \fn QAudioDecoderControl::setSourceDevice(QIODevice *device)

    Sets the current source to \a device.  Changing the source will
    stop any current decoding and discard any buffers.

    Sources are exclusive, so only one can be set.
*/

/*!
    \fn QAudioDecoderControl::start()

    Starts decoding the current media.

    If successful the player control will immediately enter the \l {QAudioDecoder::DecodingState}
    {decoding} state.

    \sa state(), read()
*/

/*!
    \fn QAudioDecoderControl::stop()

    Stops playback of the current media and discards any buffers.

    If successful the player control will immediately enter the \l {QAudioDecoder::StoppedState}
    {stopped} state.
*/

/*!
    \fn QAudioDecoderControl::error(int error, const QString &errorString)

    Signals that an \a error has occurred.  The \a errorString provides a more detailed explanation.
*/

/*!
    \fn QAudioDecoderControl::bufferAvailableChanged(bool available)

    Signals that the bufferAvailable property has changed to \a available.
*/

/*!
    \fn QAudioDecoderControl::bufferReady()

    Signals that a new buffer is ready for reading.
*/

/*!
    \fn QAudioDecoderControl::bufferAvailable() const

    Returns true if a buffer is available to be read,
    and false otherwise.
*/

/*!
    \fn QAudioDecoderControl::sourceChanged()

    Signals that the current source of the decoder has changed.

    \sa sourceFilename(), sourceDevice()
*/

/*!
    \fn QAudioDecoderControl::formatChanged(const QAudioFormat &format)

    Signals that the current audio format of the decoder has changed to \a format.

    \sa audioFormat(), setAudioFormat()
*/

/*!
    \fn void QAudioDecoderControl::finished()

    Signals that the decoding has finished successfully.
    If decoding fails, error signal is emitted instead.

    \sa start(), stop(), error()
*/

/*!
    \fn void QAudioDecoderControl::positionChanged(qint64 position)

    Signals that the current \a position of the decoder has changed.

    \sa durationChanged()
*/

/*!
    \fn void QAudioDecoderControl::durationChanged(qint64 duration)

    Signals that the estimated \a duration of the decoded data has changed.

    \sa positionChanged()
*/

/*!
    \fn QAudioDecoderControl::audioFormat() const
    Returns the current audio format of the decoded stream.

    Any buffers returned should have this format.

    \sa setAudioFormat(), formatChanged()
*/

/*!
    \fn QAudioDecoderControl::setAudioFormat(const QAudioFormat &format)
    Set the desired audio format for decoded samples to \a format.

    If the decoder does not support this format, \l error() will
    be set to \c FormatError.

    If you do not specify a format, the format of the decoded
    audio itself will be used.  Otherwise, some format conversion
    will be applied.

    If you wish to reset the decoded format to that of the original
    audio file, you can specify an invalid \a format.
*/

/*!
    \fn QAudioDecoderControl::read()
    Attempts to read a buffer from the decoder, without blocking. Returns invalid buffer if there are
    no decoded buffers available, or on error.
*/

/*!
    \fn QAudioDecoderControl::position() const
    Returns position (in milliseconds) of the last buffer read from
    the decoder or -1 if no buffers have been read.
*/

/*!
    \fn QAudioDecoderControl::duration() const
    Returns total duration (in milliseconds) of the audio stream
    or -1 if not available.
*/

#include "moc_qaudiodecodercontrol.cpp"
QT_END_NAMESPACE

