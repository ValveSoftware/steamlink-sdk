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

#include "qandroidcapturesession.h"

#include "androidcamera.h"
#include "qandroidcamerasession.h"
#include "androidmultimediautils.h"
#include "qandroidmultimediautils.h"
#include "qandroidvideooutput.h"

QT_BEGIN_NAMESPACE

QAndroidCaptureSession::QAndroidCaptureSession(QAndroidCameraSession *cameraSession)
    : QObject()
    , m_mediaRecorder(0)
    , m_cameraSession(cameraSession)
    , m_audioSource(AndroidMediaRecorder::DefaultAudioSource)
    , m_duration(0)
    , m_state(QMediaRecorder::StoppedState)
    , m_status(QMediaRecorder::UnloadedStatus)
    , m_containerFormatDirty(true)
    , m_videoSettingsDirty(true)
    , m_audioSettingsDirty(true)
    , m_outputFormat(AndroidMediaRecorder::DefaultOutputFormat)
    , m_audioEncoder(AndroidMediaRecorder::DefaultAudioEncoder)
    , m_videoEncoder(AndroidMediaRecorder::DefaultVideoEncoder)
{
    m_mediaStorageLocation.addStorageLocation(
                QMediaStorageLocation::Movies,
                AndroidMultimediaUtils::getDefaultMediaDirectory(AndroidMultimediaUtils::DCIM));

    m_mediaStorageLocation.addStorageLocation(
                QMediaStorageLocation::Sounds,
                AndroidMultimediaUtils::getDefaultMediaDirectory(AndroidMultimediaUtils::Sounds));

    connect(this, SIGNAL(stateChanged(QMediaRecorder::State)), this, SLOT(updateStatus()));

    if (cameraSession) {
        connect(cameraSession, SIGNAL(opened()), this, SLOT(onCameraOpened()));
        connect(cameraSession, SIGNAL(statusChanged(QCamera::Status)), this, SLOT(updateStatus()));
        connect(cameraSession, SIGNAL(captureModeChanged(QCamera::CaptureModes)),
                this, SLOT(updateStatus()));
        connect(cameraSession, SIGNAL(readyForCaptureChanged(bool)), this, SLOT(updateStatus()));
    } else {
        updateStatus();
    }

    m_notifyTimer.setInterval(1000);
    connect(&m_notifyTimer, SIGNAL(timeout()), this, SLOT(updateDuration()));
}

QAndroidCaptureSession::~QAndroidCaptureSession()
{
    stop();
    delete m_mediaRecorder;
}

void QAndroidCaptureSession::setAudioInput(const QString &input)
{
    if (m_audioInput == input)
        return;

    m_audioInput = input;

    if (m_audioInput == QLatin1String("default"))
        m_audioSource = AndroidMediaRecorder::DefaultAudioSource;
    else if (m_audioInput == QLatin1String("mic"))
        m_audioSource = AndroidMediaRecorder::Mic;
    else if (m_audioInput == QLatin1String("voice_uplink"))
        m_audioSource = AndroidMediaRecorder::VoiceUplink;
    else if (m_audioInput == QLatin1String("voice_downlink"))
        m_audioSource = AndroidMediaRecorder::VoiceDownlink;
    else if (m_audioInput == QLatin1String("voice_call"))
        m_audioSource = AndroidMediaRecorder::VoiceCall;
    else if (m_audioInput == QLatin1String("voice_recognition"))
        m_audioSource = AndroidMediaRecorder::VoiceRecognition;
    else
        m_audioSource = AndroidMediaRecorder::DefaultAudioSource;

    emit audioInputChanged(m_audioInput);
}

QUrl QAndroidCaptureSession::outputLocation() const
{
    return m_actualOutputLocation;
}

bool QAndroidCaptureSession::setOutputLocation(const QUrl &location)
{
    if (m_requestedOutputLocation == location)
        return false;

    m_actualOutputLocation = QUrl();
    m_requestedOutputLocation = location;

    if (m_requestedOutputLocation.isEmpty())
        return true;

    if (m_requestedOutputLocation.isValid()
            && (m_requestedOutputLocation.isLocalFile() || m_requestedOutputLocation.isRelative())) {
        return true;
    }

    m_requestedOutputLocation = QUrl();
    return false;
}

QMediaRecorder::State QAndroidCaptureSession::state() const
{
    return m_state;
}

void QAndroidCaptureSession::setState(QMediaRecorder::State state)
{
    if (m_state == state)
        return;

    switch (state) {
    case QMediaRecorder::StoppedState:
        stop();
        break;
    case QMediaRecorder::RecordingState:
        start();
        break;
    case QMediaRecorder::PausedState:
        // Not supported by Android API
        qWarning("QMediaRecorder::PausedState is not supported on Android");
        break;
    }
}

void QAndroidCaptureSession::start()
{
    if (m_state == QMediaRecorder::RecordingState || m_status != QMediaRecorder::LoadedStatus)
        return;

    setStatus(QMediaRecorder::StartingStatus);

    if (m_mediaRecorder) {
        m_mediaRecorder->release();
        delete m_mediaRecorder;
    }
    m_mediaRecorder = new AndroidMediaRecorder;
    connect(m_mediaRecorder, SIGNAL(error(int,int)), this, SLOT(onError(int,int)));
    connect(m_mediaRecorder, SIGNAL(info(int,int)), this, SLOT(onInfo(int,int)));

    // Set audio/video sources
    if (m_cameraSession) {
        updateViewfinder();
        m_cameraSession->camera()->unlock();
        m_mediaRecorder->setCamera(m_cameraSession->camera());
        m_mediaRecorder->setAudioSource(AndroidMediaRecorder::Camcorder);
        m_mediaRecorder->setVideoSource(AndroidMediaRecorder::Camera);
    } else {
        m_mediaRecorder->setAudioSource(m_audioSource);
    }

    // Set output format
    m_mediaRecorder->setOutputFormat(m_outputFormat);

    // Set audio encoder settings
    m_mediaRecorder->setAudioChannels(m_audioSettings.channelCount());
    m_mediaRecorder->setAudioEncodingBitRate(m_audioSettings.bitRate());
    m_mediaRecorder->setAudioSamplingRate(m_audioSettings.sampleRate());
    m_mediaRecorder->setAudioEncoder(m_audioEncoder);

    // Set video encoder settings
    if (m_cameraSession) {
        m_mediaRecorder->setVideoSize(m_videoSettings.resolution());
        m_mediaRecorder->setVideoFrameRate(qRound(m_videoSettings.frameRate()));
        m_mediaRecorder->setVideoEncodingBitRate(m_videoSettings.bitRate());
        m_mediaRecorder->setVideoEncoder(m_videoEncoder);

        m_mediaRecorder->setOrientationHint(m_cameraSession->currentCameraRotation());
    }

    // Set output file
    QString filePath = m_mediaStorageLocation.generateFileName(
                m_requestedOutputLocation.isLocalFile() ? m_requestedOutputLocation.toLocalFile()
                                                        : m_requestedOutputLocation.toString(),
                m_cameraSession ? QMediaStorageLocation::Movies
                                : QMediaStorageLocation::Sounds,
                m_cameraSession ? QLatin1String("VID_")
                                : QLatin1String("REC_"),
                m_containerFormat);

    m_usedOutputLocation = QUrl::fromLocalFile(filePath);
    m_mediaRecorder->setOutputFile(filePath);

    // Even though the Android doc explicitly says that calling MediaRecorder.setPreviewDisplay()
    // is not necessary when the Camera already has a Surface, it doesn't actually work on some
    // devices. For example on the Samsung Galaxy Tab 2, the camera server dies after prepare()
    // and start() if MediaRecorder.setPreviewDispaly() is not called.
    if (m_cameraSession) {
        // When using a SurfaceTexture, we need to pass a new one to the MediaRecorder, not the same
        // one that is set on the Camera or it will crash, hence the reset().
        m_cameraSession->videoOutput()->reset();
        if (m_cameraSession->videoOutput()->surfaceTexture())
            m_mediaRecorder->setSurfaceTexture(m_cameraSession->videoOutput()->surfaceTexture());
        else if (m_cameraSession->videoOutput()->surfaceHolder())
            m_mediaRecorder->setSurfaceHolder(m_cameraSession->videoOutput()->surfaceHolder());
    }

    if (!m_mediaRecorder->prepare()) {
        emit error(QMediaRecorder::FormatError, QLatin1String("Unable to prepare the media recorder."));
        if (m_cameraSession)
            restartViewfinder();
        return;
    }

    if (!m_mediaRecorder->start()) {
        emit error(QMediaRecorder::FormatError, QLatin1String("Unable to start the media recorder."));
        if (m_cameraSession)
            restartViewfinder();
        return;
    }

    m_elapsedTime.start();
    m_notifyTimer.start();
    updateDuration();

    if (m_cameraSession) {
        m_cameraSession->setReadyForCapture(false);

        // Preview frame callback is cleared when setting up the camera with the media recorder.
        // We need to reset it.
        m_cameraSession->camera()->setupPreviewFrameCallback();
    }

    m_state = QMediaRecorder::RecordingState;
    emit stateChanged(m_state);
}

void QAndroidCaptureSession::stop(bool error)
{
    if (m_state == QMediaRecorder::StoppedState || m_mediaRecorder == 0)
        return;

    setStatus(QMediaRecorder::FinalizingStatus);

    m_mediaRecorder->stop();
    m_notifyTimer.stop();
    updateDuration();
    m_elapsedTime.invalidate();
    m_mediaRecorder->release();
    delete m_mediaRecorder;
    m_mediaRecorder = 0;

    if (m_cameraSession && m_cameraSession->status() == QCamera::ActiveStatus) {
        // Viewport needs to be restarted after recording
        restartViewfinder();
    }

    if (!error) {
        // if the media is saved into the standard media location, register it
        // with the Android media scanner so it appears immediately in apps
        // such as the gallery.
        QString mediaPath = m_usedOutputLocation.toLocalFile();
        QString standardLoc = m_cameraSession ? AndroidMultimediaUtils::getDefaultMediaDirectory(AndroidMultimediaUtils::DCIM)
                                              : AndroidMultimediaUtils::getDefaultMediaDirectory(AndroidMultimediaUtils::Sounds);
        if (mediaPath.startsWith(standardLoc))
            AndroidMultimediaUtils::registerMediaFile(mediaPath);

        m_actualOutputLocation = m_usedOutputLocation;
        emit actualLocationChanged(m_actualOutputLocation);
    }

    m_state = QMediaRecorder::StoppedState;
    emit stateChanged(m_state);
}

void QAndroidCaptureSession::setStatus(QMediaRecorder::Status status)
{
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged(m_status);
}

QMediaRecorder::Status QAndroidCaptureSession::status() const
{
    return m_status;
}

qint64 QAndroidCaptureSession::duration() const
{
    return m_duration;
}

void QAndroidCaptureSession::setContainerFormat(const QString &format)
{
    if (m_containerFormat == format)
        return;

    m_containerFormat = format;
    m_containerFormatDirty = true;
}

void QAndroidCaptureSession::setAudioSettings(const QAudioEncoderSettings &settings)
{
    if (m_audioSettings == settings)
        return;

    m_audioSettings = settings;
    m_audioSettingsDirty = true;
}

void QAndroidCaptureSession::setVideoSettings(const QVideoEncoderSettings &settings)
{
    if (!m_cameraSession || m_videoSettings == settings)
        return;

    m_videoSettings = settings;
    m_videoSettingsDirty = true;
}

void QAndroidCaptureSession::applySettings()
{
    // container settings
    if (m_containerFormatDirty) {
        if (m_containerFormat.isEmpty()) {
            m_containerFormat = m_defaultSettings.outputFileExtension;
            m_outputFormat = m_defaultSettings.outputFormat;
        } else if (m_containerFormat == QLatin1String("3gp")) {
            m_outputFormat = AndroidMediaRecorder::THREE_GPP;
        } else if (!m_cameraSession && m_containerFormat == QLatin1String("amr")) {
            m_outputFormat = AndroidMediaRecorder::AMR_NB_Format;
        } else if (!m_cameraSession && m_containerFormat == QLatin1String("awb")) {
            m_outputFormat = AndroidMediaRecorder::AMR_WB_Format;
        } else {
            m_containerFormat = QStringLiteral("mp4");
            m_outputFormat = AndroidMediaRecorder::MPEG_4;
        }

        m_containerFormatDirty = false;
    }

    // audio settings
    if (m_audioSettingsDirty) {
        if (m_audioSettings.channelCount() <= 0)
            m_audioSettings.setChannelCount(m_defaultSettings.audioChannels);
        if (m_audioSettings.bitRate() <= 0)
            m_audioSettings.setBitRate(m_defaultSettings.audioBitRate);
        if (m_audioSettings.sampleRate() <= 0)
            m_audioSettings.setSampleRate(m_defaultSettings.audioSampleRate);

        if (m_audioSettings.codec().isEmpty())
            m_audioEncoder = m_defaultSettings.audioEncoder;
        else if (m_audioSettings.codec() == QLatin1String("aac"))
            m_audioEncoder = AndroidMediaRecorder::AAC;
        else if (m_audioSettings.codec() == QLatin1String("amr-nb"))
            m_audioEncoder = AndroidMediaRecorder::AMR_NB_Encoder;
        else if (m_audioSettings.codec() == QLatin1String("amr-wb"))
            m_audioEncoder = AndroidMediaRecorder::AMR_WB_Encoder;
        else
            m_audioEncoder = m_defaultSettings.audioEncoder;

        m_audioSettingsDirty = false;
    }

    // video settings
    if (m_cameraSession && m_cameraSession->camera() && m_videoSettingsDirty) {
        if (m_videoSettings.resolution().isEmpty()) {
            m_videoSettings.setResolution(m_defaultSettings.videoResolution);
        } else if (!m_supportedResolutions.contains(m_videoSettings.resolution())) {
            // if the requested resolution is not supported, find the closest one
            QSize reqSize = m_videoSettings.resolution();
            int reqPixelCount = reqSize.width() * reqSize.height();
            QList<int> supportedPixelCounts;
            for (int i = 0; i < m_supportedResolutions.size(); ++i) {
                const QSize &s = m_supportedResolutions.at(i);
                supportedPixelCounts.append(s.width() * s.height());
            }
            int closestIndex = qt_findClosestValue(supportedPixelCounts, reqPixelCount);
            m_videoSettings.setResolution(m_supportedResolutions.at(closestIndex));
        }

        if (m_videoSettings.frameRate() <= 0)
            m_videoSettings.setFrameRate(m_defaultSettings.videoFrameRate);
        if (m_videoSettings.bitRate() <= 0)
            m_videoSettings.setBitRate(m_defaultSettings.videoBitRate);

        if (m_videoSettings.codec().isEmpty())
            m_videoEncoder = m_defaultSettings.videoEncoder;
        else if (m_videoSettings.codec() == QLatin1String("h263"))
            m_videoEncoder = AndroidMediaRecorder::H263;
        else if (m_videoSettings.codec() == QLatin1String("h264"))
            m_videoEncoder = AndroidMediaRecorder::H264;
        else if (m_videoSettings.codec() == QLatin1String("mpeg4_sp"))
            m_videoEncoder = AndroidMediaRecorder::MPEG_4_SP;
        else
            m_videoEncoder = m_defaultSettings.videoEncoder;

        m_videoSettingsDirty = false;
    }
}

void QAndroidCaptureSession::updateViewfinder()
{
    m_cameraSession->camera()->stopPreviewSynchronous();
    m_cameraSession->applyViewfinderSettings(m_videoSettings.resolution(), false);
}

void QAndroidCaptureSession::restartViewfinder()
{
    if (!m_cameraSession)
        return;

    m_cameraSession->camera()->reconnect();

    // This is not necessary on most devices, but it crashes on some if we don't stop the
    // preview and reset the preview display on the camera when recording is over.
    m_cameraSession->camera()->stopPreviewSynchronous();
    m_cameraSession->videoOutput()->reset();
    if (m_cameraSession->videoOutput()->surfaceTexture())
        m_cameraSession->camera()->setPreviewTexture(m_cameraSession->videoOutput()->surfaceTexture());
    else if (m_cameraSession->videoOutput()->surfaceHolder())
        m_cameraSession->camera()->setPreviewDisplay(m_cameraSession->videoOutput()->surfaceHolder());

    m_cameraSession->camera()->startPreview();
    m_cameraSession->setReadyForCapture(true);
}

void QAndroidCaptureSession::updateDuration()
{
    if (m_elapsedTime.isValid())
        m_duration = m_elapsedTime.elapsed();

    emit durationChanged(m_duration);
}

void QAndroidCaptureSession::onCameraOpened()
{
    m_supportedResolutions.clear();
    m_supportedFramerates.clear();

    // get supported resolutions from predefined profiles
    for (int i = 0; i < 8; ++i) {
        CaptureProfile profile = getProfile(i);
        if (!profile.isNull) {
            if (i == AndroidCamcorderProfile::QUALITY_HIGH)
                m_defaultSettings = profile;

            if (!m_supportedResolutions.contains(profile.videoResolution))
                m_supportedResolutions.append(profile.videoResolution);
            if (!m_supportedFramerates.contains(profile.videoFrameRate))
                m_supportedFramerates.append(profile.videoFrameRate);
        }
    }

    qSort(m_supportedResolutions.begin(), m_supportedResolutions.end(), qt_sizeLessThan);
    qSort(m_supportedFramerates.begin(), m_supportedFramerates.end());

    applySettings();
}

QAndroidCaptureSession::CaptureProfile QAndroidCaptureSession::getProfile(int id)
{
    CaptureProfile profile;
    const bool hasProfile = AndroidCamcorderProfile::hasProfile(m_cameraSession->camera()->cameraId(),
                                                                AndroidCamcorderProfile::Quality(id));

    if (hasProfile) {
        AndroidCamcorderProfile camProfile = AndroidCamcorderProfile::get(m_cameraSession->camera()->cameraId(),
                                                                          AndroidCamcorderProfile::Quality(id));

        profile.outputFormat = AndroidMediaRecorder::OutputFormat(camProfile.getValue(AndroidCamcorderProfile::fileFormat));
        profile.audioEncoder = AndroidMediaRecorder::AudioEncoder(camProfile.getValue(AndroidCamcorderProfile::audioCodec));
        profile.audioBitRate = camProfile.getValue(AndroidCamcorderProfile::audioBitRate);
        profile.audioChannels = camProfile.getValue(AndroidCamcorderProfile::audioChannels);
        profile.audioSampleRate = camProfile.getValue(AndroidCamcorderProfile::audioSampleRate);
        profile.videoEncoder = AndroidMediaRecorder::VideoEncoder(camProfile.getValue(AndroidCamcorderProfile::videoCodec));
        profile.videoBitRate = camProfile.getValue(AndroidCamcorderProfile::videoBitRate);
        profile.videoFrameRate = camProfile.getValue(AndroidCamcorderProfile::videoFrameRate);
        profile.videoResolution = QSize(camProfile.getValue(AndroidCamcorderProfile::videoFrameWidth),
                                        camProfile.getValue(AndroidCamcorderProfile::videoFrameHeight));

        if (profile.outputFormat == AndroidMediaRecorder::MPEG_4)
            profile.outputFileExtension = QStringLiteral("mp4");
        else if (profile.outputFormat == AndroidMediaRecorder::THREE_GPP)
            profile.outputFileExtension = QStringLiteral("3gp");
        else if (profile.outputFormat == AndroidMediaRecorder::AMR_NB_Format)
            profile.outputFileExtension = QStringLiteral("amr");
        else if (profile.outputFormat == AndroidMediaRecorder::AMR_WB_Format)
            profile.outputFileExtension = QStringLiteral("awb");

        profile.isNull = false;
    }

    return profile;
}

void QAndroidCaptureSession::updateStatus()
{
    if (m_cameraSession) {
        // Video recording

        // stop recording when stopping the camera
        if (m_cameraSession->status() == QCamera::StoppingStatus
                || !m_cameraSession->captureMode().testFlag(QCamera::CaptureVideo)) {
            setState(QMediaRecorder::StoppedState);
            return;
        }

        if (m_state == QMediaRecorder::RecordingState) {
            setStatus(QMediaRecorder::RecordingStatus);
        } else if (m_cameraSession->status() == QCamera::UnavailableStatus) {
            setStatus(QMediaRecorder::UnavailableStatus);
        } else if (m_cameraSession->captureMode().testFlag(QCamera::CaptureVideo)
                   && m_cameraSession->isReadyForCapture()) {
            if (m_cameraSession->status() == QCamera::StartingStatus)
                setStatus(QMediaRecorder::LoadingStatus);
            else if (m_cameraSession->status() == QCamera::ActiveStatus)
                setStatus(QMediaRecorder::LoadedStatus);
            else
                setStatus(QMediaRecorder::UnloadedStatus);
        } else {
            setStatus(QMediaRecorder::UnloadedStatus);
        }

    } else {
        // Audio-only recording
        if (m_state == QMediaRecorder::RecordingState)
            setStatus(QMediaRecorder::RecordingStatus);
        else
            setStatus(QMediaRecorder::LoadedStatus);
    }
}

void QAndroidCaptureSession::onError(int what, int extra)
{
    Q_UNUSED(what)
    Q_UNUSED(extra)
    stop(true);
    emit error(QMediaRecorder::ResourceError, QLatin1String("Unknown error."));
}

void QAndroidCaptureSession::onInfo(int what, int extra)
{
    Q_UNUSED(extra)
    if (what == 800) {
        // MEDIA_RECORDER_INFO_MAX_DURATION_REACHED
        setState(QMediaRecorder::StoppedState);
        emit error(QMediaRecorder::OutOfSpaceError, QLatin1String("Maximum duration reached."));
    } else if (what == 801) {
        // MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED
        setState(QMediaRecorder::StoppedState);
        emit error(QMediaRecorder::OutOfSpaceError, QLatin1String("Maximum file size reached."));
    }
}

QT_END_NAMESPACE
