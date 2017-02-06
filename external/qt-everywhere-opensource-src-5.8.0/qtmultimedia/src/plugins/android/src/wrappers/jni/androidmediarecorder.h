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

#ifndef ANDROIDMEDIARECORDER_H
#define ANDROIDMEDIARECORDER_H

#include <qobject.h>
#include <QtCore/private/qjni_p.h>
#include <qsize.h>

QT_BEGIN_NAMESPACE

class AndroidCamera;
class AndroidSurfaceTexture;
class AndroidSurfaceHolder;

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

    void setSurfaceTexture(AndroidSurfaceTexture *texture);
    void setSurfaceHolder(AndroidSurfaceHolder *holder);

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
