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

#include "qaudiodecoder.h"

#include "qmediaobject_p.h"
#include <qmediaservice.h>
#include "qaudiodecodercontrol.h"
#include <private/qmediaserviceprovider_p.h>

#include <QtCore/qcoreevent.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qtimer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qpointer.h>

QT_BEGIN_NAMESPACE

/*!
    \class QAudioDecoder
    \brief The QAudioDecoder class allows decoding audio.
    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_audio

    \preliminary

    The QAudioDecoder class is a high level class for decoding local
    audio media files.  It is similar to the QMediaPlayer class except
    that audio is provided back through this API rather than routed
    directly to audio hardware, and playlists and network and streaming
    based media is not supported.

    \sa QAudioBuffer
*/

static void qRegisterAudioDecoderMetaTypes()
{
    qRegisterMetaType<QAudioDecoder::State>("QAudioDecoder::State");
    qRegisterMetaType<QAudioDecoder::Error>("QAudioDecoder::Error");
}

Q_CONSTRUCTOR_FUNCTION(qRegisterAudioDecoderMetaTypes)

class QAudioDecoderPrivate : public QMediaObjectPrivate
{
    Q_DECLARE_NON_CONST_PUBLIC(QAudioDecoder)

public:
    QAudioDecoderPrivate()
        : provider(0)
        , control(0)
        , state(QAudioDecoder::StoppedState)
        , error(QAudioDecoder::NoError)
    {}

    QMediaServiceProvider *provider;
    QAudioDecoderControl *control;
    QAudioDecoder::State state;
    QAudioDecoder::Error error;
    QString errorString;

    void _q_stateChanged(QAudioDecoder::State state);
    void _q_error(int error, const QString &errorString);
};

void QAudioDecoderPrivate::_q_stateChanged(QAudioDecoder::State ps)
{
    Q_Q(QAudioDecoder);

    if (ps != state) {
        state = ps;

        emit q->stateChanged(ps);
    }
}

void QAudioDecoderPrivate::_q_error(int error, const QString &errorString)
{
    Q_Q(QAudioDecoder);

    this->error = QAudioDecoder::Error(error);
    this->errorString = errorString;

    emit q->error(this->error);
}

/*!
    Construct an QAudioDecoder instance
    parented to \a parent.
*/
QAudioDecoder::QAudioDecoder(QObject *parent)
    : QMediaObject(*new QAudioDecoderPrivate,
                   parent,
                   QMediaServiceProvider::defaultServiceProvider()->requestService(Q_MEDIASERVICE_AUDIODECODER))
{
    Q_D(QAudioDecoder);

    d->provider = QMediaServiceProvider::defaultServiceProvider();
    if (d->service) {
        d->control = qobject_cast<QAudioDecoderControl*>(d->service->requestControl(QAudioDecoderControl_iid));
        if (d->control != 0) {
            connect(d->control, SIGNAL(stateChanged(QAudioDecoder::State)), SLOT(_q_stateChanged(QAudioDecoder::State)));
            connect(d->control, SIGNAL(error(int,QString)), SLOT(_q_error(int,QString)));

            connect(d->control, SIGNAL(formatChanged(QAudioFormat)), SIGNAL(formatChanged(QAudioFormat)));
            connect(d->control, SIGNAL(sourceChanged()), SIGNAL(sourceChanged()));
            connect(d->control, SIGNAL(bufferReady()), this, SIGNAL(bufferReady()));
            connect(d->control ,SIGNAL(bufferAvailableChanged(bool)), this, SIGNAL(bufferAvailableChanged(bool)));
            connect(d->control ,SIGNAL(finished()), this, SIGNAL(finished()));
            connect(d->control ,SIGNAL(positionChanged(qint64)), this, SIGNAL(positionChanged(qint64)));
            connect(d->control ,SIGNAL(durationChanged(qint64)), this, SIGNAL(durationChanged(qint64)));
        }
    }
    if (!d->control) {
       d->error = ServiceMissingError;
       d->errorString = tr("The QAudioDecoder object does not have a valid service");
    }
}


/*!
    Destroys the audio decoder object.
*/
QAudioDecoder::~QAudioDecoder()
{
    Q_D(QAudioDecoder);

    if (d->service) {
        if (d->control)
            d->service->releaseControl(d->control);

        d->provider->releaseService(d->service);
    }
}

QAudioDecoder::State QAudioDecoder::state() const
{
    return d_func()->state;
}

/*!
    Returns the current error state.
*/

QAudioDecoder::Error QAudioDecoder::error() const
{
    return d_func()->error;
}

QString QAudioDecoder::errorString() const
{
    return d_func()->errorString;
}

/*!
    Starts decoding the audio resource.

    As data gets decoded, the \l bufferReady() signal will be emitted
    when enough data has been decoded.  Calling \l read() will then return
    an audio buffer without blocking.

    If you call read() before a buffer is ready, an invalid buffer will
    be returned, again without blocking.

    \sa read()
*/
void QAudioDecoder::start()
{
    Q_D(QAudioDecoder);

    if (d->control == 0) {
        QMetaObject::invokeMethod(this, "_q_error", Qt::QueuedConnection,
                                    Q_ARG(int, QAudioDecoder::ServiceMissingError),
                                    Q_ARG(QString, tr("The QAudioDecoder object does not have a valid service")));
        return;
    }

    // Reset error conditions
    d->error = NoError;
    d->errorString.clear();

    d->control->start();
}

/*!
    Stop decoding audio.  Calling \l start() again will resume decoding from the beginning.
*/
void QAudioDecoder::stop()
{
    Q_D(QAudioDecoder);

    if (d->control != 0)
        d->control->stop();
}

/*!
    Returns the current file name to decode.
    If \l setSourceDevice was called, this will
    be empty.
*/
QString QAudioDecoder::sourceFilename() const
{
    Q_D(const QAudioDecoder);
    if (d->control)
        return d->control->sourceFilename();
    return QString();
}

/*!
    Sets the current audio file name to \a fileName.

    When this property is set any current decoding is stopped,
    and any audio buffers are discarded.

    You can only specify either a source filename or
    a source QIODevice.  Setting one will unset the other.
*/
void QAudioDecoder::setSourceFilename(const QString &fileName)
{
    Q_D(QAudioDecoder);

    if (d->control != 0)
        d_func()->control->setSourceFilename(fileName);
}

/*!
    Returns the current source QIODevice, if one was set.
    If \l setSourceFilename() was called, this will be 0.
*/
QIODevice *QAudioDecoder::sourceDevice() const
{
    Q_D(const QAudioDecoder);
    if (d->control)
        return d->control->sourceDevice();
    return 0;
}

/*!
    Sets the current audio QIODevice to \a device.

    When this property is set any current decoding is stopped,
    and any audio buffers are discarded.

    You can only specify either a source filename or
    a source QIODevice.  Setting one will unset the other.
*/
void QAudioDecoder::setSourceDevice(QIODevice *device)
{
    Q_D(QAudioDecoder);

    if (d->control != 0)
        d_func()->control->setSourceDevice(device);
}

/*!
    Returns the current audio format of the decoded stream.

    Any buffers returned should have this format.

    \sa setAudioFormat(), formatChanged()
*/
QAudioFormat QAudioDecoder::audioFormat() const
{
    Q_D(const QAudioDecoder);
    if (d->control)
        return d->control->audioFormat();
    return QAudioFormat();
}

/*!
    Set the desired audio format for decoded samples to \a format.

    This property can only be set while the decoder is stopped.
    Setting this property at other times will be ignored.

    If the decoder does not support this format, \l error() will
    be set to \c FormatError.

    If you do not specify a format, the format of the decoded
    audio itself will be used.  Otherwise, some format conversion
    will be applied.

    If you wish to reset the decoded format to that of the original
    audio file, you can specify an invalid \a format.
*/
void QAudioDecoder::setAudioFormat(const QAudioFormat &format)
{
    Q_D(QAudioDecoder);

    if (state() != QAudioDecoder::StoppedState)
        return;

    if (d->control != 0)
        d_func()->control->setAudioFormat(format);
}

/*!
    \internal
*/

bool QAudioDecoder::bind(QObject *obj)
{
    return QMediaObject::bind(obj);
}

/*!
    \internal
*/

void QAudioDecoder::unbind(QObject *obj)
{
    QMediaObject::unbind(obj);
}

/*!
    Returns the level of support an audio decoder has for a \a mimeType and a set of \a codecs.
*/
QMultimedia::SupportEstimate QAudioDecoder::hasSupport(const QString &mimeType,
                                               const QStringList& codecs)
{
    return QMediaServiceProvider::defaultServiceProvider()->hasSupport(QByteArray(Q_MEDIASERVICE_AUDIODECODER),
                                                                    mimeType,
                                                                    codecs);
}

/*!
    Returns true if a buffer is available to be read,
    and false otherwise.  If there is no buffer available, calling
    the \l read() function will return an invalid buffer.
*/
bool QAudioDecoder::bufferAvailable() const
{
    Q_D(const QAudioDecoder);
    if (d->control)
        return d->control->bufferAvailable();
    return false;
}

/*!
    Returns position (in milliseconds) of the last buffer read from
    the decoder or -1 if no buffers have been read.
*/

qint64 QAudioDecoder::position() const
{
    Q_D(const QAudioDecoder);
    if (d->control)
        return d->control->position();
    return -1;
}

/*!
    Returns total duration (in milliseconds) of the audio stream or -1
    if not available.
*/

qint64 QAudioDecoder::duration() const
{
    Q_D(const QAudioDecoder);
    if (d->control)
        return d->control->duration();
    return -1;
}

/*!
    Read a buffer from the decoder, if one is available. Returns an invalid buffer
    if there are no decoded buffers currently available, or on failure.  In both cases
    this function will not block.

    You should either respond to the \l bufferReady() signal or check the
    \l bufferAvailable() function before calling read() to make sure
    you get useful data.
*/

QAudioBuffer QAudioDecoder::read() const
{
    Q_D(const QAudioDecoder);

    if (d->control) {
        return d->control->read();
    } else {
        return QAudioBuffer();
    }
}

// Enums
/*!
    \enum QAudioDecoder::State

    Defines the current state of a media player.

    \value StoppedState The decoder is not decoding.  Decoding will
           start at the start of the media.
    \value DecodingState The audio player is currently decoding media.
*/

/*!
    \enum QAudioDecoder::Error

    Defines a media player error condition.

    \value NoError No error has occurred.
    \value ResourceError A media resource couldn't be resolved.
    \value FormatError The format of a media resource isn't supported.
    \value AccessDeniedError There are not the appropriate permissions to play a media resource.
    \value ServiceMissingError A valid playback service was not found, playback cannot proceed.
*/

// Signals
/*!
    \fn QAudioDecoder::error(QAudioDecoder::Error error)

    Signals that an \a error condition has occurred.

    \sa errorString()
*/

/*!
    \fn void QAudioDecoder::stateChanged(State state)

    Signal the \a state of the decoder object has changed.
*/

/*!
    \fn void QAudioDecoder::sourceChanged()

    Signals that the current source of the decoder has changed.

    \sa sourceFilename(), sourceDevice()
*/

/*!
    \fn void QAudioDecoder::formatChanged(const QAudioFormat &format)

    Signals that the current audio format of the decoder has changed to \a format.

    \sa audioFormat(), setAudioFormat()
*/

/*!
    \fn void QAudioDecoder::bufferReady()

    Signals that a new decoded audio buffer is available to be read.

    \sa read(), bufferAvailable()
*/

/*!
    \fn void QAudioDecoder::bufferAvailableChanged(bool available)

    Signals the availability (if \a available is true) of a new buffer.

    If \a available is false, there are no buffers available.

    \sa bufferAvailable(), bufferReady()
*/

/*!
    \fn void QAudioDecoder::finished()

    Signals that the decoding has finished successfully.
    If decoding fails, error signal is emitted instead.

    \sa start(), stop(), error()
*/

/*!
    \fn void QAudioDecoder::positionChanged(qint64 position)

    Signals that the current \a position of the decoder has changed.

    \sa durationChanged()
*/

/*!
    \fn void QAudioDecoder::durationChanged(qint64 duration)

    Signals that the estimated \a duration of the decoded data has changed.

    \sa positionChanged()
*/


// Properties
/*!
    \property QAudioDecoder::state
    \brief the audio decoder's playback state.

    By default this property is QAudioDecoder::Stopped

    \sa start(), stop()
*/

/*!
    \property QAudioDecoder::error
    \brief a string describing the last error condition.

    \sa error()
*/

/*!
    \property QAudioDecoder::sourceFilename
    \brief the active filename being decoded by the decoder object.
*/

/*!
    \property QAudioDecoder::bufferAvailable
    \brief whether there is a decoded audio buffer available
*/

#include "moc_qaudiodecoder.cpp"
QT_END_NAMESPACE

