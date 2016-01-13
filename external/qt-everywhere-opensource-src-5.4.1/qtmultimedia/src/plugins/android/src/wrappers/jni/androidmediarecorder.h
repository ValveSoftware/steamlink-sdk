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

#ifndef ANDROIDMEDIARECORDER_H
#define ANDROIDMEDIARECORDER_H

#include <qobject.h>
#include <QtCore/private/qjni_p.h>
#include <qsize.h>

QT_BEGIN_NAMESPACE

class AndroidCamera;

class AndroidCamcorderProfile
{
public:
    enum Quality { // Needs to match CamcorderProfile
        QUALITY_LOW,
        QUALITY_HIGH,
        QUALITY_QCIF,
        QUALITY_CIF,
        QUALITY_480P,
        QUALITY_720P,
        QUALITY_1080P,
        QUALITY_QVGA
    };

    enum Field {
        audioBitRate,
        audioChannels,
        audioCodec,
        audioSampleRate,
        duration,
        fileFormat,
        quality,
        videoBitRate,
        videoCodec,
        videoFrameHeight,
        videoFrameRate,
        videoFrameWidth
    };

    static bool hasProfile(jint cameraId, Quality quality);
    static AndroidCamcorderProfile get(jint cameraId, Quality quality);
    int getValue(Field field) const;

private:
    AndroidCamcorderProfile(const QJNIObjectPrivate &camcorderProfile);
    QJNIObjectPrivate m_camcorderProfile;
};

class AndroidMediaRecorder : public QObject
{
    Q_OBJECT
public:
    enum AudioEncoder {
        DefaultAudioEncoder = 0,
        AMR_NB_Encoder = 1,
        AMR_WB_Encoder = 2,
        AAC = 3
    };

    enum AudioSource {
        DefaultAudioSource = 0,
        Mic = 1,
        VoiceUplink = 2,
        VoiceDownlink = 3,
        VoiceCall = 4,
        Camcorder = 5,
        VoiceRecognition = 6
    };

    enum VideoEncoder {
        DefaultVideoEncoder = 0,
        H263 = 1,
        H264 = 2,
        MPEG_4_SP = 3
    };

    enum VideoSource {
        DefaultVideoSource = 0,
        Camera = 1
    };

    enum OutputFormat {
        DefaultOutputFormat = 0,
        THREE_GPP = 1,
        MPEG_4 = 2,
        AMR_NB_Format = 3,
        AMR_WB_Format = 4
    };

    AndroidMediaRecorder();
    ~AndroidMediaRecorder();

    void release();
    bool prepare();
    void reset();

    bool start();
    void stop();

    void setAudioChannels(int numChannels);
    void setAudioEncoder(AudioEncoder encoder);
    void setAudioEncodingBitRate(int bitRate);
    void setAudioSamplingRate(int samplingRate);
    void setAudioSource(AudioSource source);

    void setCamera(AndroidCamera *camera);
    void setVideoEncoder(VideoEncoder encoder);
    void setVideoEncodingBitRate(int bitRate);
    void setVideoFrameRate(int rate);
    void setVideoSize(const QSize &size);
    void setVideoSource(VideoSource source);

    void setOrientationHint(int degrees);

    void setOutputFormat(OutputFormat format);
    void setOutputFile(const QString &path);

    static bool initJNI(JNIEnv *env);

Q_SIGNALS:
    void error(int what, int extra);
    void info(int what, int extra);

private:
    jlong m_id;
    QJNIObjectPrivate m_mediaRecorder;
};

QT_END_NAMESPACE

#endif // ANDROIDMEDIARECORDER_H
