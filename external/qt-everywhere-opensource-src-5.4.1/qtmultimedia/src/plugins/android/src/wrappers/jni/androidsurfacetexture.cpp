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

#include "androidsurfacetexture.h"
#include <QtCore/private/qjni_p.h>
#include <QtCore/private/qjnihelpers_p.h>

QT_BEGIN_NAMESPACE

static const char QtSurfaceTextureListenerClassName[] = "org/qtproject/qt5/android/multimedia/QtSurfaceTextureListener";
static QMap<int, AndroidSurfaceTexture*> g_objectMap;

// native method for QtSurfaceTexture.java
static void notifyFrameAvailable(JNIEnv* , jobject, int id)
{
    AndroidSurfaceTexture *obj = g_objectMap.value(id, 0);
    if (obj)
        Q_EMIT obj->frameAvailable();
}

AndroidSurfaceTexture::AndroidSurfaceTexture(unsigned int texName)
    : QObject()
    , m_texID(int(texName))
{
    // API level 11 or higher is required
    if (QtAndroidPrivate::androidSdkVersion() < 11) {
        qWarning("Camera preview and video playback require Android 3.0 (API level 11) or later.");
        return;
    }

    QJNIEnvironmentPrivate env;
    m_surfaceTexture = QJNIObjectPrivate("android/graphics/SurfaceTexture", "(I)V", jint(texName));
    if (env->ExceptionCheck()) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif // QT_DEBUG
        env->ExceptionClear();
    }

    if (m_surfaceTexture.isValid())
        g_objectMap.insert(int(texName), this);

    QJNIObjectPrivate listener(QtSurfaceTextureListenerClassName, "(I)V", jint(texName));
    m_surfaceTexture.callMethod<void>("setOnFrameAvailableListener",
                                      "(Landroid/graphics/SurfaceTexture$OnFrameAvailableListener;)V",
                                      listener.object());
}

AndroidSurfaceTexture::~AndroidSurfaceTexture()
{
    if (QtAndroidPrivate::androidSdkVersion() > 13 && m_surfaceView.isValid())
        m_surfaceView.callMethod<void>("release");

    if (m_surfaceTexture.isValid()) {
        release();
        g_objectMap.remove(m_texID);
    }
}

QMatrix4x4 AndroidSurfaceTexture::getTransformMatrix()
{
    QMatrix4x4 matrix;
    if (!m_surfaceTexture.isValid())
        return matrix;

    QJNIEnvironmentPrivate env;

    jfloatArray array = env->NewFloatArray(16);
    m_surfaceTexture.callMethod<void>("getTransformMatrix", "([F)V", array);
    env->GetFloatArrayRegion(array, 0, 16, matrix.data());
    env->DeleteLocalRef(array);

    return matrix;
}

void AndroidSurfaceTexture::release()
{
    if (QtAndroidPrivate::androidSdkVersion() < 14)
        return;

    m_surfaceTexture.callMethod<void>("release");
}

void AndroidSurfaceTexture::updateTexImage()
{
    if (!m_surfaceTexture.isValid())
        return;

    m_surfaceTexture.callMethod<void>("updateTexImage");
}

jobject AndroidSurfaceTexture::surfaceTexture()
{
    return m_surfaceTexture.object();
}

jobject AndroidSurfaceTexture::surfaceView()
{
    return m_surfaceView.object();
}

jobject AndroidSurfaceTexture::surfaceHolder()
{
    if (!m_surfaceHolder.isValid()) {
        m_surfaceView = QJNIObjectPrivate("android/view/Surface",
                                          "(Landroid/graphics/SurfaceTexture;)V",
                                          m_surfaceTexture.object());

        m_surfaceHolder = QJNIObjectPrivate("org/qtproject/qt5/android/multimedia/QtSurfaceTextureHolder",
                                            "(Landroid/view/Surface;)V",
                                            m_surfaceView.object());
    }

    return m_surfaceHolder.object();
}

bool AndroidSurfaceTexture::initJNI(JNIEnv *env)
{
    // SurfaceTexture is available since API 11.
    if (QtAndroidPrivate::androidSdkVersion() < 11)
        return false;

    jclass clazz = QJNIEnvironmentPrivate::findClass(QtSurfaceTextureListenerClassName,
                                                     env);

    static const JNINativeMethod methods[] = {
        {"notifyFrameAvailable", "(I)V", (void *)notifyFrameAvailable}
    };

    if (clazz && env->RegisterNatives(clazz,
                                      methods,
                                      sizeof(methods) / sizeof(methods[0])) != JNI_OK) {
        return false;
    }

    return true;
}

QT_END_NAMESPACE
