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

#include "qgstreamerrecordercontrol.h"
#include "qgstreameraudioencode.h"
#include "qgstreamervideoencode.h"
#include "qgstreamermediacontainercontrol.h"
#include <QtCore/QDebug>
#include <QtGui/qdesktopservices.h>

QGstreamerRecorderControl::QGstreamerRecorderControl(QGstreamerCaptureSession *session)
    :QMediaRecorderControl(session),
      m_session(session),
      m_state(QMediaRecorder::StoppedState),
      m_status(QMediaRecorder::UnloadedStatus)
{
    connect(m_session, SIGNAL(stateChanged(QGstreamerCaptureSession::State)), SLOT(updateStatus()));
    connect(m_session, SIGNAL(error(int,QString)), SLOT(handleSessionError(int,QString)));
    connect(m_session, SIGNAL(durationChanged(qint64)), SIGNAL(durationChanged(qint64)));
    connect(m_session, SIGNAL(mutedChanged(bool)), SIGNAL(mutedChanged(bool)));
    connect(m_session, SIGNAL(volumeChanged(qreal)), SIGNAL(volumeChanged(qreal)));
    m_hasPreviewState = m_session->captureMode() != QGstreamerCaptureSession::Audio;
}

QGstreamerRecorderControl::~QGstreamerRecorderControl()
{
}

QUrl QGstreamerRecorderControl::outputLocation() const
{
    return m_session->outputLocation();
}

bool QGstreamerRecorderControl::setOutputLocation(const QUrl &sink)
{
    m_outputLocation = sink;
    m_session->setOutputLocation(sink);
    return true;
}


QMediaRecorder::State QGstreamerRecorderControl::state() const
{
    return m_state;
}

QMediaRecorder::Status QGstreamerRecorderControl::status() const
{
    static QMediaRecorder::Status statusTable[3][3] = {
        //Stopped recorder state:
        { QMediaRecorder::LoadedStatus, QMediaRecorder::FinalizingStatus, QMediaRecorder::FinalizingStatus },
        //Recording recorder state:
        { QMediaRecorder::StartingStatus, QMediaRecorder::RecordingStatus, QMediaRecorder::PausedStatus },
        //Paused recorder state:
        { QMediaRecorder::StartingStatus, QMediaRecorder::RecordingStatus, QMediaRecorder::PausedStatus }
    };

    QMediaRecorder::State sessionState = QMediaRecorder::StoppedState;

    switch ( m_session->state() ) {
        case QGstreamerCaptureSession::RecordingState:
            sessionState = QMediaRecorder::RecordingState;
            break;
        case QGstreamerCaptureSession::PausedState:
            sessionState = QMediaRecorder::PausedState;
            break;
        case QGstreamerCaptureSession::PreviewState:
        case QGstreamerCaptureSession::StoppedState:
            sessionState = QMediaRecorder::StoppedState;
            break;
    }

    return statusTable[m_state][sessionState];
}

void QGstreamerRecorderControl::updateStatus()
{
    QMediaRecorder::Status newStatus = status();
    if (m_status != newStatus) {
        m_status = newStatus;
        emit statusChanged(m_status);
    }
}

void QGstreamerRecorderControl::handleSessionError(int code, const QString &description)
{
    emit error(code, description);
    stop();
}

qint64 QGstreamerRecorderControl::duration() const
{
    return m_session->duration();
}

void QGstreamerRecorderControl::setState(QMediaRecorder::State state)
{
    switch (state) {
    case QMediaRecorder::StoppedState:
        stop();
        break;
    case QMediaRecorder::PausedState:
        pause();
        break;
    case QMediaRecorder::RecordingState:
        record();
        break;
    }
}

void QGstreamerRecorderControl::record()
{
    if (m_state == QMediaRecorder::RecordingState)
        return;

    m_state = QMediaRecorder::RecordingState;

    if (m_outputLocation.isEmpty()) {
        QString container = m_session->mediaContainerControl()->containerExtension();
        if (container.isEmpty())
            container = "raw";

        m_session->setOutputLocation(QUrl(generateFileName(defaultDir(), container)));
    }

    m_session->dumpGraph("before-record");
    if (!m_hasPreviewState || m_session->state() != QGstreamerCaptureSession::StoppedState) {
        m_session->setState(QGstreamerCaptureSession::RecordingState);
    } else
        emit error(QMediaRecorder::ResourceError, tr("Service has not been started"));

    m_session->dumpGraph("after-record");

    emit stateChanged(m_state);
    updateStatus();

    emit actualLocationChanged(m_session->outputLocation());
}

void QGstreamerRecorderControl::pause()
{
    if (m_state == QMediaRecorder::PausedState)
        return;

    m_state = QMediaRecorder::PausedState;

    m_session->dumpGraph("before-pause");
    if (!m_hasPreviewState || m_session->state() != QGstreamerCaptureSession::StoppedState) {
        m_session->setState(QGstreamerCaptureSession::PausedState);
    } else
        emit error(QMediaRecorder::ResourceError, tr("Service has not been started"));

    emit stateChanged(m_state);
    updateStatus();
}

void QGstreamerRecorderControl::stop()
{
    if (m_state == QMediaRecorder::StoppedState)
        return;

    m_state = QMediaRecorder::StoppedState;

    if (!m_hasPreviewState) {
        m_session->setState(QGstreamerCaptureSession::StoppedState);
    } else {
        if (m_session->state() != QGstreamerCaptureSession::StoppedState)
            m_session->setState(QGstreamerCaptureSession::PreviewState);
    }

    emit stateChanged(m_state);
    updateStatus();
}

void QGstreamerRecorderControl::applySettings()
{
    //Check the codecs are compatible with container,
    //and choose the compatible codecs/container if omitted
    QGstreamerAudioEncode *audioEncodeControl = m_session->audioEncodeControl();
    QGstreamerVideoEncode *videoEncodeControl = m_session->videoEncodeControl();
    QGstreamerMediaContainerControl *mediaContainerControl = m_session->mediaContainerControl();

    bool needAudio = m_session->captureMode() & QGstreamerCaptureSession::Audio;
    bool needVideo = m_session->captureMode() & QGstreamerCaptureSession::Video;

    QStringList containerCandidates;
    if (mediaContainerControl->containerFormat().isEmpty())
        containerCandidates = mediaContainerControl->supportedContainers();
    else
        containerCandidates << mediaContainerControl->containerFormat();


    QStringList audioCandidates;
    if (needAudio) {
        QAudioEncoderSettings audioSettings = audioEncodeControl->audioSettings();
        if (audioSettings.codec().isEmpty())
            audioCandidates = audioEncodeControl->supportedAudioCodecs();
        else
            audioCandidates << audioSettings.codec();
    }

    QStringList videoCandidates;
    if (needVideo) {
        QVideoEncoderSettings videoSettings = videoEncodeControl->videoSettings();
        if (videoSettings.codec().isEmpty())
            videoCandidates = videoEncodeControl->supportedVideoCodecs();
        else
            videoCandidates << videoSettings.codec();
    }

    QString container;
    QString audioCodec;
    QString videoCodec;

    for (const QString &containerCandidate : qAsConst(containerCandidates)) {
        QSet<QString> supportedTypes = mediaContainerControl->supportedStreamTypes(containerCandidate);

        audioCodec.clear();
        videoCodec.clear();

        if (needAudio) {
            bool found = false;
            for (const QString &audioCandidate : qAsConst(audioCandidates)) {
                QSet<QString> audioTypes = audioEncodeControl->supportedStreamTypes(audioCandidate);
                if (audioTypes.intersects(supportedTypes)) {
                    found = true;
                    audioCodec = audioCandidate;
                    break;
                }
            }
            if (!found)
                continue;
        }

        if (needVideo) {
            bool found = false;
            for (const QString &videoCandidate : qAsConst(videoCandidates)) {
                QSet<QString> videoTypes = videoEncodeControl->supportedStreamTypes(videoCandidate);
                if (videoTypes.intersects(supportedTypes)) {
                    found = true;
                    videoCodec = videoCandidate;
                    break;
                }
            }
            if (!found)
                continue;
        }

        container = containerCandidate;
        break;
    }

    if (container.isEmpty()) {
        emit error(QMediaRecorder::FormatError, tr("Not compatible codecs and container format."));
    } else {
        mediaContainerControl->setContainerFormat(container);

        if (needAudio) {
            QAudioEncoderSettings audioSettings = audioEncodeControl->audioSettings();
            audioSettings.setCodec(audioCodec);
            audioEncodeControl->setAudioSettings(audioSettings);
        }

        if (needVideo) {
            QVideoEncoderSettings videoSettings = videoEncodeControl->videoSettings();
            videoSettings.setCodec(videoCodec);
            videoEncodeControl->setVideoSettings(videoSettings);
        }
    }
}


bool QGstreamerRecorderControl::isMuted() const
{
    return m_session->isMuted();
}

qreal QGstreamerRecorderControl::volume() const
{
    return m_session->volume();
}

void QGstreamerRecorderControl::setMuted(bool muted)
{
    m_session->setMuted(muted);
}

void QGstreamerRecorderControl::setVolume(qreal volume)
{
    m_session->setVolume(volume);
}

QDir QGstreamerRecorderControl::defaultDir() const
{
    QStringList dirCandidates;

    if (m_session->captureMode() & QGstreamerCaptureSession::Video)
        dirCandidates << QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    else
        dirCandidates << QStandardPaths::writableLocation(QStandardPaths::MusicLocation);

    dirCandidates << QDir::home().filePath("Documents");
    dirCandidates << QDir::home().filePath("My Documents");
    dirCandidates << QDir::homePath();
    dirCandidates << QDir::currentPath();
    dirCandidates << QDir::tempPath();

    for (const QString &path : qAsConst(dirCandidates)) {
        QDir dir(path);
        if (dir.exists() && QFileInfo(path).isWritable())
            return dir;
    }

    return QDir();
}

QString QGstreamerRecorderControl::generateFileName(const QDir &dir, const QString &ext) const
{

    int lastClip = 0;
    const auto list = dir.entryList(QStringList() << QString("clip_*.%1").arg(ext));
    for (const QString &fileName : list) {
        int imgNumber = fileName.midRef(5, fileName.size()-6-ext.length()).toInt();
        lastClip = qMax(lastClip, imgNumber);
    }

    QString name = QString("clip_%1.%2").arg(lastClip+1,
                                     4, //fieldWidth
                                     10,
                                     QLatin1Char('0')).arg(ext);

    return dir.absoluteFilePath(name);
}
