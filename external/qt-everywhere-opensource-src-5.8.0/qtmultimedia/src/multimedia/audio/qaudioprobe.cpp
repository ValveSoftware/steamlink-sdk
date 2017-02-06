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

/*!
    \class QAudioProbe
    \inmodule QtMultimedia

    \ingroup multimedia
    \ingroup multimedia_audio

    \brief The QAudioProbe class allows you to monitor audio being played or recorded.

    \code
        QAudioRecorder *recorder = new QAudioRecorder();
        QAudioProbe *probe = new QAudioProbe;

        // ... configure the audio recorder (skipped)

        connect(probe, SIGNAL(audioBufferProbed(QAudioBuffer)), this, SLOT(processBuffer(QAudioBuffer)));

        probe->setSource(recorder); // Returns true, hopefully.

        recorder->record(); // Now we can do things like calculating levels or performing an FFT
    \endcode

    \sa QVideoProbe, QMediaPlayer, QCamera
*/

#include "qaudioprobe.h"
#include "qmediaaudioprobecontrol.h"
#include "qmediaservice.h"
#include "qmediarecorder.h"
#include "qsharedpointer.h"
#include "qpointer.h"

QT_BEGIN_NAMESPACE

class QAudioProbePrivate {
public:
    QPointer<QMediaObject> source;
    QPointer<QMediaAudioProbeControl> probee;
};

/*!
    Creates a new QAudioProbe class with a \a parent.  After setting the
    source to monitor with \l setSource(), the \l audioBufferProbed()
    signal will be emitted when audio buffers are flowing in the
    source media object.
 */
QAudioProbe::QAudioProbe(QObject *parent)
    : QObject(parent)
    , d(new QAudioProbePrivate)
{
}

/*!
    Destroys this probe and disconnects from any
    media object.
 */
QAudioProbe::~QAudioProbe()
{
    if (d->source) {
        // Disconnect
        if (d->probee) {
            disconnect(d->probee.data(), SIGNAL(audioBufferProbed(QAudioBuffer)), this, SIGNAL(audioBufferProbed(QAudioBuffer)));
            disconnect(d->probee.data(), SIGNAL(flush()), this, SIGNAL(flush()));
        }
        d->source.data()->service()->releaseControl(d->probee.data());
    }
}

/*!
    Sets the media object to monitor to \a source.

    If \a source is zero, this probe will be deactivated
    and this function wil return true.

    If the media object does not support monitoring
    audio, this function will return false.

    The previous object will no longer be monitored.
    Passing in the same object will be ignored, but
    monitoring will continue.
 */
bool QAudioProbe::setSource(QMediaObject *source)
{
    // Need to:
    // 1) disconnect from current source if necessary
    // 2) see if new one has the probe control
    // 3) connect if so

    // in case source was destroyed but probe control is still valid
    if (!d->source && d->probee) {
        disconnect(d->probee.data(), SIGNAL(audioBufferProbed(QAudioBuffer)), this, SIGNAL(audioBufferProbed(QAudioBuffer)));
        disconnect(d->probee.data(), SIGNAL(flush()), this, SIGNAL(flush()));
        d->probee.clear();
    }

    if (source != d->source.data()) {
        if (d->source) {
            Q_ASSERT(d->probee);
            disconnect(d->probee.data(), SIGNAL(audioBufferProbed(QAudioBuffer)), this, SIGNAL(audioBufferProbed(QAudioBuffer)));
            disconnect(d->probee.data(), SIGNAL(flush()), this, SIGNAL(flush()));
            d->source.data()->service()->releaseControl(d->probee.data());
            d->source.clear();
            d->probee.clear();
        }

        if (source) {
            QMediaService *service = source->service();
            if (service) {
                d->probee = service->requestControl<QMediaAudioProbeControl*>();
            }

            if (d->probee) {
                connect(d->probee.data(), SIGNAL(audioBufferProbed(QAudioBuffer)), this, SIGNAL(audioBufferProbed(QAudioBuffer)));
                connect(d->probee.data(), SIGNAL(flush()), this, SIGNAL(flush()));
                d->source = source;
            }
        }
    }

    return (!source || d->probee != 0);
}

/*!
    Starts monitoring the given \a mediaRecorder.

    Returns true on success.

    If there is no mediaObject associated with \a mediaRecorder, or if it is
    zero, this probe will be deactivated and this function wil return true.

    If the media recorder instance does not support monitoring
    audio, this function will return false.

    Any previously monitored objects will no longer be monitored.
    Passing in the same (valid) object will be ignored, but
    monitoring will continue.
 */
bool QAudioProbe::setSource(QMediaRecorder *mediaRecorder)
{
    QMediaObject *source = mediaRecorder ? mediaRecorder->mediaObject() : 0;
    bool result = setSource(source);

    if (!mediaRecorder)
        return true;

    if (mediaRecorder && !source)
        return false;

    return result;
}

/*!
    Returns true if this probe is monitoring something, or false otherwise.

    The source being monitored does not need to be active.
 */
bool QAudioProbe::isActive() const
{
    return d->probee != 0;
}

/*!
    \fn QAudioProbe::audioBufferProbed(const QAudioBuffer &buffer)

    This signal should be emitted when an audio \a buffer is processed in the
    media service.
*/


/*!
    \fn QAudioProbe::flush()

    This signal should be emitted when it is required to release all buffers.
    Application must release all outstanding references to audio buffers.
*/

QT_END_NAMESPACE
