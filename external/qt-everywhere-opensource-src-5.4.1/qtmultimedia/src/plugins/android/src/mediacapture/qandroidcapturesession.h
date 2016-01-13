/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QANDROIDCAPTURESESSION_H
#define QANDROIDCAPTURESESSION_H

#include <qobject.h>
#include <qmediarecorder.h>
#include <qurl.h>
#include <qelapsedtimer.h>
#include <qtimer.h>
#include <private/qmediastoragelocation_p.h>
#include "androidmediarecorder.h"

QT_BEGIN_NAMESPACE

class QAndroidCameraSession;

class QAndroidCaptureSession : public QObject
{
    Q_OBJECT
public:
    explicit QAndroidCaptureSession(QAndroidCameraSession *cameraSession = 0);
    ~QAndroidCaptureSession();

    QList<QSize> supportedResolutions() const { return m_supportedResolutions; }
    QList<qreal> supportedFrameRates() const { return m_supportedFramerates; }

    QString audioInput() const { return m_audioInput; }
    void setAudioInput(const QString &input);

    QUrl outputLocation() const;
    bool setOutputLocation(const QUrl &location);

    QMediaRecorder::State state() const;
    void setState(QMediaRecorder::State state);

    QMediaRecorder::Status status() const;

    qint64 duration() const;

    QString containerFormat() const { return m_containerFormat; }
    void setContainerFormat(const QString &format);

    QAudioEncoderSettings audioSettings() const { return m_audioSettings; }
    void setAudioSettings(const QAudioEncoderSettings &settings);

    QVideoEncoderSettings videoSettings() const { return m_videoSettings; }
    void setVideoSettings(const QVideoEncoderSettings &settings);

    void applySettings();

Q_SIGNALS:
    void audioInputChanged(const QString& name);
    void stateChanged(QMediaRecorder::State state);
    void statusChanged(QMediaRecorder::Status status);
    void durationChanged(qint64 position);
    void actualLocationChanged(const QUrl &location);
    void error(int error, const QString &errorString);

private Q_SLOTS:
    void updateDuration();
    void onCameraOpened();
    void updateStatus();

    void onError(int what, int extra);
    void onInfo(int what, int extra);

private:
    struct CaptureProfile {
        AndroidMediaRecorder::OutputFormat outputFormat;
        QString outputFileExtension;

        AndroidMediaRecorder::AudioEncoder audioEncoder;
        int audioBitRate;
        int audioChannels;
        int audioSampleRate;

        AndroidMediaRecorder::VideoEncoder videoEncoder;
        int videoBitRate;
        int videoFrameRate;
        QSize videoResolution;

        bool isNull;

        CaptureProfile()
            : outputFormat(AndroidMediaRecorder::MPEG_4)
            , outputFileExtension(QLatin1String("mp4"))
            , audioEncoder(AndroidMediaRecorder::DefaultAudioEncoder)
            , audioBitRate(128000)
            , audioChannels(2)
            , audioSampleRate(44100)
            , videoEncoder(AndroidMediaRecorder::DefaultVideoEncoder)
            , videoBitRate(1)
            , videoFrameRate(-1)
            , videoResolution(320, 240)
            , isNull(true)
        { }
    };

    CaptureProfile getProfile(int id);

    bool start();
    void stop(bool error = false);

    void setStatus(QMediaRecorder::Status status);

    void updateViewfinder();
    void restartViewfinder();

    AndroidMediaRecorder *m_mediaRecorder;
    QAndroidCameraSession *m_cameraSession;

    QString m_audioInput;
    AndroidMediaRecorder::AudioSource m_audioSource;

    QMediaStorageLocation m_mediaStorageLocation;

    QElapsedTimer m_elapsedTime;
    QTimer m_notifyTimer;
    qint64 m_duration;

    QMediaRecorder::State m_state;
    QMediaRecorder::Status m_status;
    QUrl m_requestedOutputLocation;
    QUrl m_usedOutputLocation;
    QUrl m_actualOutputLocation;

    CaptureProfile m_defaultSettings;

    QString m_containerFormat;
    QAudioEncoderSettings m_audioSettings;
    QVideoEncoderSettings m_videoSettings;
    bool m_resolutionDirty;
    bool m_containerFormatDirty;
    bool m_videoSettingsDirty;
    bool m_audioSettingsDirty;
    AndroidMediaRecorder::OutputFormat m_outputFormat;
    AndroidMediaRecorder::AudioEncoder m_audioEncoder;
    AndroidMediaRecorder::VideoEncoder m_videoEncoder;

    QList<QSize> m_supportedResolutions;
    QList<qreal> m_supportedFramerates;
};

QT_END_NAMESPACE

#endif // QANDROIDCAPTURESESSION_H
