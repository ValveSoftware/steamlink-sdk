/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
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

/* Media related snippets */
#include <QFile>
#include <QTimer>

#include "qmediaplaylist.h"
#include "qmediarecorder.h"
#include "qmediaservice.h"
#include "qmediaplayercontrol.h"
#include "qmediaplayer.h"
#include "qradiotuner.h"
#include "qradiodata.h"
#include "qvideowidget.h"
#include "qcameraimagecapture.h"
#include "qcamera.h"
#include "qcameraviewfinder.h"
#include "qaudioprobe.h"
#include "qaudiorecorder.h"
#include "qvideoprobe.h"

class MediaExample : public QObject {
    Q_OBJECT

    void MediaControl();
    void MediaPlayer();
    void RadioTuna();
    void MediaRecorder();
    void AudioRecorder();
    void EncoderSettings();
    void ImageEncoderSettings();
    void AudioProbe();
    void VideoProbe();

private:
    // Common naming
    QMediaService *mediaService;
    QVideoWidget *videoWidget;
    QWidget *widget;
    QMediaPlayer *player;
    QMediaPlaylist *playlist;
    QMediaContent video;
    QMediaRecorder *recorder;
    QCamera *camera;
    QCameraViewfinder *viewfinder;
    QCameraImageCapture *imageCapture;
    QString fileName;
    QRadioTuner *radio;
    QRadioData *radioData;
    QAudioRecorder *audioRecorder;
    QAudioProbe *audioProbe;
    QVideoProbe *videoProbe;

    QMediaContent image1;
    QMediaContent image2;
    QMediaContent image3;

    static const int yourRadioStationFrequency = 11;
};

void MediaExample::MediaControl()
{
    {
    //! [Request control]
    QMediaPlayerControl *control = qobject_cast<QMediaPlayerControl *>(
            mediaService->requestControl("org.qt-project.qt.mediaplayercontrol/5.0"));
    //! [Request control]
    Q_UNUSED(control);
    }

    {
    //! [Request control templated]
    QMediaPlayerControl *control = mediaService->requestControl<QMediaPlayerControl *>();
    //! [Request control templated]
    Q_UNUSED(control);
    }
}


void MediaExample::EncoderSettings()
{
    //! [Audio encoder settings]
    QAudioEncoderSettings audioSettings;
    audioSettings.setCodec("audio/mpeg");
    audioSettings.setChannelCount(2);

    recorder->setAudioSettings(audioSettings);
    //! [Audio encoder settings]

    //! [Video encoder settings]
    QVideoEncoderSettings videoSettings;
    videoSettings.setCodec("video/mpeg2");
    videoSettings.setResolution(640, 480);

    recorder->setVideoSettings(videoSettings);
    //! [Video encoder settings]
}

void MediaExample::ImageEncoderSettings()
{
    //! [Image encoder settings]
    QImageEncoderSettings imageSettings;
    imageSettings.setCodec("image/jpeg");
    imageSettings.setResolution(1600, 1200);

    imageCapture->setEncodingSettings(imageSettings);
    //! [Image encoder settings]
}

void MediaExample::MediaPlayer()
{
    //! [Player]
    player = new QMediaPlayer;
    connect(player, SIGNAL(positionChanged(qint64)), this, SLOT(positionChanged(qint64)));
    player->setMedia(QUrl::fromLocalFile("/Users/me/Music/coolsong.mp3"));
    player->setVolume(50);
    player->play();
    //! [Player]

    //! [Local playback]
    player = new QMediaPlayer;
    // ...
    player->setMedia(QUrl::fromLocalFile("/Users/me/Music/coolsong.mp3"));
    player->setVolume(50);
    player->play();
    //! [Local playback]

    //! [Audio playlist]
    player = new QMediaPlayer;

    playlist = new QMediaPlaylist(player);
    playlist->addMedia(QUrl("http://example.com/myfile1.mp3"));
    playlist->addMedia(QUrl("http://example.com/myfile2.mp3"));
    // ...
    playlist->setCurrentIndex(1);
    player->play();
    //! [Audio playlist]

    //! [Movie playlist]
    playlist = new QMediaPlaylist;
    playlist->addMedia(QUrl("http://example.com/movie1.mp4"));
    playlist->addMedia(QUrl("http://example.com/movie2.mp4"));
    playlist->addMedia(QUrl("http://example.com/movie3.mp4"));
    playlist->setCurrentIndex(1);

    player = new QMediaPlayer;
    player->setPlaylist(playlist);

    videoWidget = new QVideoWidget;
    player->setVideoOutput(videoWidget);
    videoWidget->show();

    player->play();
    //! [Movie playlist]
}

void MediaExample::MediaRecorder()
{
    //! [Media recorder]
    recorder = new QMediaRecorder(camera);

    QAudioEncoderSettings audioSettings;
    audioSettings.setCodec("audio/amr");
    audioSettings.setQuality(QMultimedia::HighQuality);

    recorder->setAudioSettings(audioSettings);

    recorder->setOutputLocation(QUrl::fromLocalFile(fileName));
    recorder->record();
    //! [Media recorder]
}

void MediaExample::AudioRecorder()
{
    //! [Audio recorder]
    audioRecorder = new QAudioRecorder;

    QAudioEncoderSettings audioSettings;
    audioSettings.setCodec("audio/amr");
    audioSettings.setQuality(QMultimedia::HighQuality);

    audioRecorder->setEncodingSettings(audioSettings);

    audioRecorder->setOutputLocation(QUrl::fromLocalFile("test.amr"));
    audioRecorder->record();
    //! [Audio recorder]

    //! [Audio recorder inputs]
    QStringList inputs = audioRecorder->audioInputs();
    QString selectedInput = audioRecorder->defaultAudioInput();

    foreach (QString input, inputs) {
        QString description = audioRecorder->audioInputDescription(input);
        // show descriptions to user and allow selection
        selectedInput = input;
    }

    audioRecorder->setAudioInput(selectedInput);
    //! [Audio recorder inputs]
}

void MediaExample::RadioTuna()
{
    //! [Radio tuner]
    radio = new QRadioTuner;
    connect(radio, SIGNAL(frequencyChanged(int)), this, SLOT(freqChanged(int)));
    if (radio->isBandSupported(QRadioTuner::FM)) {
        radio->setBand(QRadioTuner::FM);
        radio->setFrequency(yourRadioStationFrequency);
        radio->setVolume(100);
        radio->start();
    }
    //! [Radio tuner]

    //! [Radio data setup]
    radio = new QRadioTuner;
    radioData = radio->radioData();
    //! [Radio data setup]
}

void MediaExample::AudioProbe()
{
    //! [Audio probe]
    audioRecorder = new QAudioRecorder;

    QAudioEncoderSettings audioSettings;
    audioSettings.setCodec("audio/amr");
    audioSettings.setQuality(QMultimedia::HighQuality);

    audioRecorder->setEncodingSettings(audioSettings);

    audioRecorder->setOutputLocation(QUrl::fromLocalFile("test.amr"));

    audioProbe = new QAudioProbe(this);
    if (audioProbe->setSource(audioRecorder)) {
        // Probing succeeded, audioProbe->isValid() should be true.
        connect(audioProbe, SIGNAL(audioBufferProbed(QAudioBuffer)),
                this, SLOT(calculateLevel(QAudioBuffer)));
    }

    audioRecorder->record();
    // Now audio buffers being recorded should be signaled
    // by the probe, so we can do things like calculating the
    // audio power level, or performing a frequency transform
    //! [Audio probe]
}

void MediaExample::VideoProbe()
{
    //! [Video probe]
    camera = new QCamera;
    viewfinder = new QCameraViewfinder();
    camera->setViewfinder(viewfinder);

    camera->setCaptureMode(QCamera::CaptureVideo);

    videoProbe = new QVideoProbe(this);

    if (videoProbe->setSource(camera)) {
        // Probing succeeded, videoProbe->isValid() should be true.
        connect(videoProbe, SIGNAL(videoFrameProbed(QVideoFrame)),
                this, SLOT(detectBarcodes(QVideoFrame)));
    }

    camera->start();
    // Viewfinder frames should now also be emitted by
    // the video probe, even in still image capture mode.
    // Another alternative is to install the probe on a
    // QMediaRecorder connected to the camera to get the
    // recorded frames, if they are different from the
    // viewfinder frames.

    //! [Video probe]
}


