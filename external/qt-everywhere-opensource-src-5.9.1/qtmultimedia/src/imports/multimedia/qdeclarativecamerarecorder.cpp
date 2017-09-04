/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qdeclarativecamera_p.h"
#include "qdeclarativecamerarecorder_p.h"

#include <QtCore/qurl.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype CameraRecorder
    \instantiates QDeclarativeCameraRecorder
    \inqmlmodule QtMultimedia
    \brief Controls video recording with the Camera.
    \ingroup multimedia_qml
    \ingroup camera_qml

    CameraRecorder allows recording camera streams to files, and adjusting recording
    settings and metadata for videos.

    It should not be constructed separately, instead the
    \c videoRecorder property of a \l Camera should be used.

    \qml
    Camera {
        videoRecorder.audioEncodingMode: CameraRecorder.ConstantBitrateEncoding;
        videoRecorder.audioBitRate: 128000
        videoRecorder.mediaContainer: "mp4"
        // ...
    }
    \endqml

    There are many different settings for each part of the recording process (audio,
    video, and output formats), as well as control over muting and where to store
    the output file.

    \sa QAudioEncoderSettings, QVideoEncoderSettings
*/

QDeclarativeCameraRecorder::QDeclarativeCameraRecorder(QCamera *camera, QObject *parent) :
    QObject(parent)
{
    m_recorder = new QMediaRecorder(camera, this);
    connect(m_recorder, SIGNAL(stateChanged(QMediaRecorder::State)),
            SLOT(updateRecorderState(QMediaRecorder::State)));
    connect(m_recorder, SIGNAL(statusChanged(QMediaRecorder::Status)),
            SIGNAL(recorderStatusChanged()));
    connect(m_recorder, SIGNAL(error(QMediaRecorder::Error)),
            SLOT(updateRecorderError(QMediaRecorder::Error)));
    connect(m_recorder, SIGNAL(mutedChanged(bool)), SIGNAL(mutedChanged(bool)));
    connect(m_recorder, SIGNAL(durationChanged(qint64)), SIGNAL(durationChanged(qint64)));
    connect(m_recorder, SIGNAL(actualLocationChanged(QUrl)),
            SLOT(updateActualLocation(QUrl)));
    connect(m_recorder, SIGNAL(metaDataChanged(QString,QVariant)),
            SIGNAL(metaDataChanged(QString,QVariant)));
}

QDeclarativeCameraRecorder::~QDeclarativeCameraRecorder()
{
}

/*!
    \qmlproperty size QtMultimedia::CameraRecorder::resolution

    This property holds the video frame dimensions to be used for video capture.
*/
QSize QDeclarativeCameraRecorder::captureResolution()
{
    return m_videoSettings.resolution();
}

/*!
    \qmlproperty string QtMultimedia::CameraRecorder::audioCodec

    This property holds the audio codec to be used for recording video.
    Typically this is \c aac or \c amr-wb.

    \sa {QtMultimedia::CameraImageProcessing::whiteBalanceMode}{whileBalanceMode}
*/
QString QDeclarativeCameraRecorder::audioCodec() const
{
    return m_audioSettings.codec();
}

/*!
    \qmlproperty string QtMultimedia::CameraRecorder::videoCodec

    This property holds the video codec to be used for recording video.
    Typically this is \c h264.
*/
QString QDeclarativeCameraRecorder::videoCodec() const
{
    return m_videoSettings.codec();
}

/*!
    \qmlproperty string QtMultimedia::CameraRecorder::mediaContainer

    This property holds the media container to be used for recording video.
    Typically this is \c mp4.
*/
QString QDeclarativeCameraRecorder::mediaContainer() const
{
    return m_mediaContainer;
}

void QDeclarativeCameraRecorder::setCaptureResolution(const QSize &resolution)
{
    m_videoSettings = m_recorder->videoSettings();
    if (resolution != captureResolution()) {
        m_videoSettings.setResolution(resolution);
        m_recorder->setVideoSettings(m_videoSettings);
        emit captureResolutionChanged(resolution);
    }
}

void QDeclarativeCameraRecorder::setAudioCodec(const QString &codec)
{
    m_audioSettings = m_recorder->audioSettings();
    if (codec != audioCodec()) {
        m_audioSettings.setCodec(codec);
        m_recorder->setAudioSettings(m_audioSettings);
        emit audioCodecChanged(codec);
    }
}

void QDeclarativeCameraRecorder::setVideoCodec(const QString &codec)
{
    m_videoSettings = m_recorder->videoSettings();
    if (codec != videoCodec()) {
        m_videoSettings.setCodec(codec);
        m_recorder->setVideoSettings(m_videoSettings);
        emit videoCodecChanged(codec);
    }
}

void QDeclarativeCameraRecorder::setMediaContainer(const QString &container)
{
    if (container != m_mediaContainer) {
        m_mediaContainer = container;
        m_recorder->setContainerFormat(container);
        emit mediaContainerChanged(container);
    }
}

/*!
    \qmlproperty qreal QtMultimedia::CameraRecorder::frameRate

    This property holds the framerate (in frames per second) to be used for recording video.
*/
qreal QDeclarativeCameraRecorder::frameRate() const
{
    return m_videoSettings.frameRate();
}

/*!
    \qmlproperty int QtMultimedia::CameraRecorder::videoBitRate

    This property holds the bit rate (in bits per second) to be used for recording video.
*/
int QDeclarativeCameraRecorder::videoBitRate() const
{
    return m_videoSettings.bitRate();
}

/*!
    \qmlproperty int QtMultimedia::CameraRecorder::audioBitRate

    This property holds the audio bit rate (in bits per second) to be used for recording video.
*/
int QDeclarativeCameraRecorder::audioBitRate() const
{
    return m_audioSettings.bitRate();
}

/*!
    \qmlproperty int QtMultimedia::CameraRecorder::audioChannels

    This property indicates the number of audio channels to be encoded while
    recording video (1 is mono, 2 is stereo).
*/
int QDeclarativeCameraRecorder::audioChannels() const
{
    return m_audioSettings.channelCount();
}

/*!
    \qmlproperty int QtMultimedia::CameraRecorder::audioSampleRate

    This property holds the sample rate to be used to encode audio while recording video.
*/
int QDeclarativeCameraRecorder::audioSampleRate() const
{
    return m_audioSettings.sampleRate();
}

/*!
    \qmlproperty enumeration QtMultimedia::CameraRecorder::videoEncodingMode

    This property holds the type of encoding method to be used for recording video.

    The following are the different encoding methods used:

    \table
    \header \li Value \li Description
    \row \li ConstantQualityEncoding
         \li Encoding will aim to have a constant quality, adjusting bitrate to fit.
            This is the default.  The bitrate setting will be ignored.
    \row \li ConstantBitRateEncoding
         \li Encoding will use a constant bit rate, adjust quality to fit.  This is
            appropriate if you are trying to optimize for space.
    \row \li AverageBitRateEncoding
         \li Encoding will try to keep an average bitrate setting, but will use
            more or less as needed.
    \endtable

*/
QDeclarativeCameraRecorder::EncodingMode QDeclarativeCameraRecorder::videoEncodingMode() const
{
    return EncodingMode(m_videoSettings.encodingMode());
}

/*!
    \qmlproperty enumeration QtMultimedia::CameraRecorder::audioEncodingMode

    The type of encoding method to use when recording audio.

    \table
    \header \li Value \li Description
    \row \li ConstantQualityEncoding
         \li Encoding will aim to have a constant quality, adjusting bitrate to fit.
            This is the default.  The bitrate setting will be ignored.
    \row \li ConstantBitRateEncoding
         \li Encoding will use a constant bit rate, adjust quality to fit.  This is
            appropriate if you are trying to optimize for space.
    \row \li AverageBitRateEncoding
         \li Encoding will try to keep an average bitrate setting, but will use
            more or less as needed.
    \endtable
*/
QDeclarativeCameraRecorder::EncodingMode QDeclarativeCameraRecorder::audioEncodingMode() const
{
    return EncodingMode(m_audioSettings.encodingMode());
}

void QDeclarativeCameraRecorder::setFrameRate(qreal frameRate)
{
    m_videoSettings = m_recorder->videoSettings();
    if (!qFuzzyCompare(m_videoSettings.frameRate(),frameRate)) {
        m_videoSettings.setFrameRate(frameRate);
        m_recorder->setVideoSettings(m_videoSettings);
        emit frameRateChanged(frameRate);
    }
}

void QDeclarativeCameraRecorder::setVideoBitRate(int rate)
{
    m_videoSettings = m_recorder->videoSettings();
    if (m_videoSettings.bitRate() != rate) {
        m_videoSettings.setBitRate(rate);
        m_recorder->setVideoSettings(m_videoSettings);
        emit videoBitRateChanged(rate);
    }
}

void QDeclarativeCameraRecorder::setAudioBitRate(int rate)
{
    m_audioSettings = m_recorder->audioSettings();
    if (m_audioSettings.bitRate() != rate) {
        m_audioSettings.setBitRate(rate);
        m_recorder->setAudioSettings(m_audioSettings);
        emit audioBitRateChanged(rate);
    }
}

void QDeclarativeCameraRecorder::setAudioChannels(int channels)
{
    m_audioSettings = m_recorder->audioSettings();
    if (m_audioSettings.channelCount() != channels) {
        m_audioSettings.setChannelCount(channels);
        m_recorder->setAudioSettings(m_audioSettings);
        emit audioChannelsChanged(channels);
    }
}

void QDeclarativeCameraRecorder::setAudioSampleRate(int rate)
{
    m_audioSettings = m_recorder->audioSettings();
    if (m_audioSettings.sampleRate() != rate) {
        m_audioSettings.setSampleRate(rate);
        m_recorder->setAudioSettings(m_audioSettings);
        emit audioSampleRateChanged(rate);
    }
}

void QDeclarativeCameraRecorder::setAudioEncodingMode(QDeclarativeCameraRecorder::EncodingMode encodingMode)
{
    m_audioSettings = m_recorder->audioSettings();
    if (m_audioSettings.encodingMode() != QMultimedia::EncodingMode(encodingMode)) {
        m_audioSettings.setEncodingMode(QMultimedia::EncodingMode(encodingMode));
        m_recorder->setAudioSettings(m_audioSettings);
        emit audioEncodingModeChanged(encodingMode);
    }
}

void QDeclarativeCameraRecorder::setVideoEncodingMode(QDeclarativeCameraRecorder::EncodingMode encodingMode)
{
    m_videoSettings = m_recorder->videoSettings();
    if (m_videoSettings.encodingMode() != QMultimedia::EncodingMode(encodingMode)) {
        m_videoSettings.setEncodingMode(QMultimedia::EncodingMode(encodingMode));
        m_recorder->setVideoSettings(m_videoSettings);
        emit videoEncodingModeChanged(encodingMode);
    }
}

/*!
    \qmlproperty enumeration QtMultimedia::CameraRecorder::errorCode

    This property holds the last error code.

    \table
    \header \li Value \li Description
    \row \li NoError
         \li No Errors

    \row \li ResourceError
         \li Device is not ready or not available.

    \row \li FormatError
         \li Current format is not supported.

    \row \li OutOfSpaceError
         \li No space left on device.

    \endtable
*/
QDeclarativeCameraRecorder::Error QDeclarativeCameraRecorder::errorCode() const
{
    return QDeclarativeCameraRecorder::Error(m_recorder->error());
}

/*!
    \qmlproperty string QtMultimedia::CameraRecorder::errorString

    This property holds the description of the last error.
*/
QString QDeclarativeCameraRecorder::errorString() const
{
    return m_recorder->errorString();
}

/*!
    \qmlproperty enumeration QtMultimedia::CameraRecorder::recorderState

    This property holds the current state of the camera recorder object.

    The state can be one of these two:

    \table
    \header \li Value \li Description
    \row \li StoppedState
         \li The camera is not recording video.

    \row \li RecordingState
         \li The camera is recording video.
    \endtable
*/
QDeclarativeCameraRecorder::RecorderState QDeclarativeCameraRecorder::recorderState() const
{
    //paused state is not supported for camera
    QMediaRecorder::State state = m_recorder->state();

    if (state == QMediaRecorder::PausedState)
        state = QMediaRecorder::StoppedState;

    return RecorderState(state);
}


/*!
    \qmlproperty enumeration QtMultimedia::CameraRecorder::recorderStatus

    This property holds the current status of media recording.

    \table
    \header \li Value \li Description
    \row \li UnavailableStatus
         \li Recording is not supported by the camera.
    \row \li UnloadedStatus
         \li The recorder is available but not loaded.
    \row \li LoadingStatus
         \li The recorder is initializing.
    \row \li LoadedStatus
         \li The recorder is initialized and ready to record media.
    \row \li StartingStatus
         \li Recording is requested but not active yet.
    \row \li RecordingStatus
         \li Recording is active.
    \row \li PausedStatus
         \li Recording is paused.
    \row \li FinalizingStatus
         \li Recording is stopped with media being finalized.
    \endtable
*/

QDeclarativeCameraRecorder::RecorderStatus QDeclarativeCameraRecorder::recorderStatus() const
{
    return RecorderStatus(m_recorder->status());
}

/*!
    \qmlmethod QtMultimedia::CameraRecorder::record()

    Starts recording.
*/
void QDeclarativeCameraRecorder::record()
{
    setRecorderState(RecordingState);
}

/*!
    \qmlmethod QtMultimedia::CameraRecorder::stop()

    Stops recording.
*/
void QDeclarativeCameraRecorder::stop()
{
    setRecorderState(StoppedState);
}

void QDeclarativeCameraRecorder::setRecorderState(QDeclarativeCameraRecorder::RecorderState state)
{
    if (!m_recorder)
        return;

    switch (state) {
    case QDeclarativeCameraRecorder::RecordingState:
        m_recorder->record();
        break;
    case QDeclarativeCameraRecorder::StoppedState:
        m_recorder->stop();
        break;
    }
}
/*!
    \property QDeclarativeCameraRecorder::outputLocation

    This property holds the destination location of the media content. If it is empty,
    the recorder uses the system-specific place and file naming scheme.
*/
/*!
    \qmlproperty string QtMultimedia::CameraRecorder::outputLocation

    This property holds the destination location of the media content. If the location is empty,
    the recorder uses the system-specific place and file naming scheme.
*/

QString QDeclarativeCameraRecorder::outputLocation() const
{
    return m_recorder->outputLocation().toString();
}
/*!
    \property QDeclarativeCameraRecorder::actualLocation

    This property holds the absolute location to the last saved media content.
    The location is usually available after recording starts, and reset when
    new location is set or new recording starts.
*/
/*!
    \qmlproperty string QtMultimedia::CameraRecorder::actualLocation

    This property holds the actual location of the last saved media content. The actual location is
    usually available after the recording starts, and reset when new location is set or the new recording starts.
*/

QString QDeclarativeCameraRecorder::actualLocation() const
{
    return m_recorder->actualLocation().toString();
}

void QDeclarativeCameraRecorder::setOutputLocation(const QString &location)
{
    if (outputLocation() != location) {
        m_recorder->setOutputLocation(location);
        emit outputLocationChanged(outputLocation());
    }
}
/*!
    \property QDeclarativeCameraRecorder::duration

    This property holds the duration (in miliseconds) of the last recording.
*/
/*!
    \qmlproperty int QtMultimedia::CameraRecorder::duration

   This property holds the duration (in miliseconds) of the last recording.
*/
qint64 QDeclarativeCameraRecorder::duration() const
{
    return m_recorder->duration();
}
/*!
    \property QDeclarativeCameraRecorder::muted

    This property indicates whether the audio input is muted during
    recording.
*/
/*!
    \qmlproperty bool QtMultimedia::CameraRecorder::muted

    This property indicates whether the audio input is muted during recording.
*/
bool QDeclarativeCameraRecorder::isMuted() const
{
    return m_recorder->isMuted();
}

void QDeclarativeCameraRecorder::setMuted(bool muted)
{
    m_recorder->setMuted(muted);
}

/*!
    \qmlmethod QtMultimedia::CameraRecorder::setMetadata(key, value)

    Sets metadata for the next video to be recorder, with
    the given \a key being associated with \a value.
*/
void QDeclarativeCameraRecorder::setMetadata(const QString &key, const QVariant &value)
{
    m_recorder->setMetaData(key, value);
}

void QDeclarativeCameraRecorder::updateRecorderState(QMediaRecorder::State state)
{
    if (state == QMediaRecorder::PausedState)
        state = QMediaRecorder::StoppedState;

    emit recorderStateChanged(RecorderState(state));
}

void QDeclarativeCameraRecorder::updateRecorderError(QMediaRecorder::Error errorCode)
{
    qWarning() << "QMediaRecorder error:" << errorString();
    emit error(Error(errorCode), errorString());
}

void QDeclarativeCameraRecorder::updateActualLocation(const QUrl &url)
{
    emit actualLocationChanged(url.toString());
}

QT_END_NAMESPACE

#include "moc_qdeclarativecamerarecorder_p.cpp"
