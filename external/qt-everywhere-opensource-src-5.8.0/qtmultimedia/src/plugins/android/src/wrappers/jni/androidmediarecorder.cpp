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

#include "androidmediarecorder.h"

#include "androidcamera.h"
#include "androidsurfacetexture.h"
#include "androidsurfaceview.h"
#include <QtCore/private/qjni_p.h>
#include <qmap.h>

QT_BEGIN_NAMESPACE

typedef QMap<QString, QJNIObjectPrivate> CamcorderProfiles;
Q_GLOBAL_STATIC(CamcorderProfiles, g_camcorderProfiles)

static QString profileKey()
{
    return QStringLiteral("%1-%2");
}

bool AndroidCamcorderProfile::hasProfile(jint cameraId, Quality quality)
{
    if (g_camcorderProfiles->contains(profileKey().arg(cameraId).arg(quality)))
        return true;

    return QJNIObjectPrivate::callStaticMethod<jboolean>("android/media/CamcorderProfile",
                                                         "hasProfile",
                                                         "(II)Z",
                                                         cameraId,
                                                         quality);
}

AndroidCamcorderProfile AndroidCamcorderProfile::get(jint cameraId, Quality quality)
{
    const QString key = profileKey().arg(cameraId).arg(quality);
    QMap<QString, QJNIObjectPrivate>::const_iterator it = g_camcorderProfiles->constFind(key);

    if (it != g_camcorderProfiles->constEnd())
        return AndroidCamcorderProfile(*it);

    QJNIObjectPrivate camProfile = QJNIObjectPrivate::callStaticObjectMethod("android/media/CamcorderProfile",
                                                                             "get",
                                                                             "(II)Landroid/media/CamcorderProfile;",
                                                                             cameraId,
                                                                             quality);

    return AndroidCamcorderProfile((*g_camcorderProfiles)[key] = camProfile);
}

int AndroidCamcorderProfile::getValue(AndroidCamcorderProfile::Field field) const
{
    switch (field) {
    case audioBitRate:
        return m_camcorderProfile.getField<jint>("audioBitRate");
    case audioChannels:
        return m_camcorderProfile.getField<jint>("audioChannels");
    case audioCodec:
        return m_camcorderProfile.getField<jint>("audioCodec");
    case audioSampleRate:
        return m_camcorderProfile.getField<jint>("audioSampleRate");
    case duration:
        return m_camcorderProfile.getField<jint>("duration");
    case fileFormat:
        return m_camcorderProfile.getField<jint>("fileFormat");
    case quality:
        return m_camcorderProfile.getField<jint>("quality");
    case videoBitRate:
        return m_camcorderProfile.getField<jint>("videoBitRate");
    case videoCodec:
        return m_camcorderProfile.getField<jint>("videoCodec");
    case videoFrameHeight:
        return m_camcorderProfile.getField<jint>("videoFrameHeight");
    case videoFrameRate:
        return m_camcorderProfile.getField<jint>("videoFrameRate");
    case videoFrameWidth:
        return m_camcorderProfile.getField<jint>("videoFrameWidth");
    }

    return 0;
}

AndroidCamcorderProfile::AndroidCamcorderProfile(const QJNIObjectPrivate &camcorderProfile)
{
    m_camcorderProfile = camcorderProfile;
}

static const char QtMediaRecorderListenerClassName[] = "org/qtproject/qt5/android/multimedia/QtMediaRecorderListener";
typedef QMap<jlong, AndroidMediaRecorder*> MediaRecorderMap;
Q_GLOBAL_STATIC(MediaRecorderMap, mediaRecorders)

static void notifyError(JNIEnv* , jobject, jlong id, jint what, jint extra)
{
    AndroidMediaRecorder *obj = mediaRecorders->value(id, 0);
    if (obj)
        emit obj->error(what, extra);
}

static void notifyInfo(JNIEnv* , jobject, jlong id, jint what, jint extra)
{
    AndroidMediaRecorder *obj = mediaRecorders->value(id, 0);
    if (obj)
        emit obj->info(what, extra);
}

AndroidMediaRecorder::AndroidMediaRecorder()
    : QObject()
    , m_id(reinterpret_cast<jlong>(this))
{
    m_mediaRecorder = QJNIObjectPrivate("android/media/MediaRecorder");
    if (m_mediaRecorder.isValid()) {
        QJNIObjectPrivate listener(QtMediaRecorderListenerClassName, "(J)V", m_id);
        m_mediaRecorder.callMethod<void>("setOnErrorListener",
                                         "(Landroid/media/MediaRecorder$OnErrorListener;)V",
                                         listener.object());
        m_mediaRecorder.callMethod<void>("setOnInfoListener",
                                         "(Landroid/media/MediaRecorder$OnInfoListener;)V",
                                         listener.object());
        mediaRecorders->insert(m_id, this);
    }
}

AndroidMediaRecorder::~AndroidMediaRecorder()
{
    mediaRecorders->remove(m_id);
}

void AndroidMediaRecorder::release()
{
    m_mediaRecorder.callMethod<void>("release");
}

bool AndroidMediaRecorder::prepare()
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("prepare");
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
        return false;
    }
    return true;
}

void AndroidMediaRecorder::reset()
{
    m_mediaRecorder.callMethod<void>("reset");
}

bool AndroidMediaRecorder::start()
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("start");
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
        return false;
    }
    return true;
}

void AndroidMediaRecorder::stop()
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("stop");
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
}

void AndroidMediaRecorder::setAudioChannels(int numChannels)
{
    m_mediaRecorder.callMethod<void>("setAudioChannels", "(I)V", numChannels);
}

void AndroidMediaRecorder::setAudioEncoder(AudioEncoder encoder)
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("setAudioEncoder", "(I)V", int(encoder));
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
}

void AndroidMediaRecorder::setAudioEncodingBitRate(int bitRate)
{
    m_mediaRecorder.callMethod<void>("setAudioEncodingBitRate", "(I)V", bitRate);
}

void AndroidMediaRecorder::setAudioSamplingRate(int samplingRate)
{
    m_mediaRecorder.callMethod<void>("setAudioSamplingRate", "(I)V", samplingRate);
}

void AndroidMediaRecorder::setAudioSource(AudioSource source)
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("setAudioSource", "(I)V", int(source));
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
}

void AndroidMediaRecorder::setCamera(AndroidCamera *camera)
{
    QJNIObjectPrivate cam = camera->getCameraObject();
    m_mediaRecorder.callMethod<void>("setCamera", "(Landroid/hardware/Camera;)V", cam.object());
}

void AndroidMediaRecorder::setVideoEncoder(VideoEncoder encoder)
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("setVideoEncoder", "(I)V", int(encoder));
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
}

void AndroidMediaRecorder::setVideoEncodingBitRate(int bitRate)
{
    m_mediaRecorder.callMethod<void>("setVideoEncodingBitRate", "(I)V", bitRate);
}

void AndroidMediaRecorder::setVideoFrameRate(int rate)
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("setVideoFrameRate", "(I)V", rate);
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
}

void AndroidMediaRecorder::setVideoSize(const QSize &size)
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("setVideoSize", "(II)V", size.width(), size.height());
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
}

void AndroidMediaRecorder::setVideoSource(VideoSource source)
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("setVideoSource", "(I)V", int(source));
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
}

void AndroidMediaRecorder::setOrientationHint(int degrees)
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("setOrientationHint", "(I)V", degrees);
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
}

void AndroidMediaRecorder::setOutputFormat(OutputFormat format)
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("setOutputFormat", "(I)V", int(format));
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
}

void AndroidMediaRecorder::setOutputFile(const QString &path)
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("setOutputFile",
                                     "(Ljava/lang/String;)V",
                                     QJNIObjectPrivate::fromString(path).object());
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
}

void AndroidMediaRecorder::setSurfaceTexture(AndroidSurfaceTexture *texture)
{
    QJNIEnvironmentPrivate env;
    m_mediaRecorder.callMethod<void>("setPreviewDisplay",
                                     "(Landroid/view/Surface;)V",
                                     texture->surface());
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
}

void AndroidMediaRecorder::setSurfaceHolder(AndroidSurfaceHolder *holder)
{
    QJNIEnvironmentPrivate env;
    QJNIObjectPrivate surfaceHolder(holder->surfaceHolder());
    QJNIObjectPrivate surface = surfaceHolder.callObjectMethod("getSurface",
                                                               "()Landroid/view/Surface;");
    if (!surface.isValid())
        return;

    m_mediaRecorder.callMethod<void>("setPreviewDisplay",
                                     "(Landroid/view/Surface;)V",
                                     surface.object());
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif
        env->ExceptionClear();
    }
}


bool AndroidMediaRecorder::initJNI(JNIEnv *env)
{
    jclass clazz = QJNIEnvironmentPrivate::findClass(QtMediaRecorderListenerClassName,
                                                     env);

    static const JNINativeMethod methods[] = {
        {"notifyError", "(JII)V", (void *)notifyError},
        {"notifyInfo", "(JII)V", (void *)notifyInfo}
    };

    if (clazz && env->RegisterNatives(clazz,
                                      methods,
                                      sizeof(methods) / sizeof(methods[0])) != JNI_OK) {
            return false;
    }

    return true;
}

QT_END_NAMESPACE
