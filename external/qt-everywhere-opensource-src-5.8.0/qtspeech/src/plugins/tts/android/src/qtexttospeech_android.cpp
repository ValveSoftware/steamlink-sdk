/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Speech module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "qtexttospeech_android.h"

#include <jni.h>
#include <QtCore/private/qjnihelpers_p.h>

QT_BEGIN_NAMESPACE

static jclass g_qtSpeechClass = 0;

typedef QMap<jlong, QTextToSpeechEngineAndroid *> TextToSpeechMap;
Q_GLOBAL_STATIC(TextToSpeechMap, textToSpeechMap)

static void notifyError(JNIEnv *env, jobject thiz, jlong id)
{
    Q_UNUSED(env);
    Q_UNUSED(thiz);

    QTextToSpeechEngineAndroid *const tts = (*textToSpeechMap)[id];
    if (!tts)
        return;

    QMetaObject::invokeMethod(tts, "processNotifyError", Qt::AutoConnection);
}

static void notifyReady(JNIEnv *env, jobject thiz, jlong id)
{
    Q_UNUSED(env);
    Q_UNUSED(thiz);

    QTextToSpeechEngineAndroid *const tts = (*textToSpeechMap)[id];
    if (!tts)
        return;

    QMetaObject::invokeMethod(tts, "processNotifyReady", Qt::AutoConnection);
}

static void notifySpeaking(JNIEnv *env, jobject thiz, jlong id)
{
    Q_UNUSED(env);
    Q_UNUSED(thiz);

    QTextToSpeechEngineAndroid *const tts = (*textToSpeechMap)[id];
    if (!tts)
        return;

    QMetaObject::invokeMethod(tts, "processNotifySpeaking", Qt::AutoConnection);
}

Q_DECL_EXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void */*reserved*/)
{
    static bool initialized = false;
    if (initialized)
        return JNI_VERSION_1_6;
    initialized = true;

    typedef union {
        JNIEnv *nativeEnvironment;
        void *venv;
    } UnionJNIEnvToVoid;

    UnionJNIEnvToVoid uenv;
    uenv.venv = NULL;

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_6) != JNI_OK)
        return JNI_ERR;

    JNIEnv *jniEnv = uenv.nativeEnvironment;
    jclass clazz = jniEnv->FindClass("org/qtproject/qt5/android/speech/QtTextToSpeech");

    static const JNINativeMethod methods[] = {
        {"notifyError", "(J)V", reinterpret_cast<void *>(notifyError)},
        {"notifyReady", "(J)V", reinterpret_cast<void *>(notifyReady)},
        {"notifySpeaking", "(J)V", reinterpret_cast<void *>(notifySpeaking)}
    };

    if (clazz) {
        g_qtSpeechClass = static_cast<jclass>(jniEnv->NewGlobalRef(clazz));
        if (jniEnv->RegisterNatives(g_qtSpeechClass,
                                    methods,
                                    sizeof(methods) / sizeof(methods[0])) != JNI_OK) {
            return JNI_ERR;
        }
    }

    return JNI_VERSION_1_6;
}

QTextToSpeechEngineAndroid::QTextToSpeechEngineAndroid(const QVariantMap &parameters, QObject *parent)
    : QTextToSpeechEngine(parent)
    , m_speech()
    , m_state(QTextToSpeech::Ready)
    , m_text()
{
    Q_UNUSED(parameters)
    Q_ASSERT(g_qtSpeechClass);

    const jlong id = reinterpret_cast<jlong>(this);
    m_speech = QJNIObjectPrivate::callStaticObjectMethod(g_qtSpeechClass,
                                                         "open",
                                                         "(Landroid/content/Context;J)Lorg/qtproject/qt5/android/speech/QtTextToSpeech;",
                                                         QtAndroidPrivate::context(),
                                                         id);
    (*textToSpeechMap)[id] = this;
}

QTextToSpeechEngineAndroid::~QTextToSpeechEngineAndroid()
{
    textToSpeechMap->remove(reinterpret_cast<jlong>(this));
    m_speech.callMethod<void>("shutdown");
}

void QTextToSpeechEngineAndroid::say(const QString &text)
{
    if (text.isEmpty())
        return;

    if (m_state == QTextToSpeech::Speaking)
        stop();

    m_text = text;
    QJNIEnvironmentPrivate env;
    jstring jstr = env->NewString(reinterpret_cast<const jchar*>(text.constData()),
                                  text.length());
    m_speech.callMethod<void>("say", "(Ljava/lang/String;)V", jstr);
}

QTextToSpeech::State QTextToSpeechEngineAndroid::state() const
{
    return m_state;
}

void QTextToSpeechEngineAndroid::setState(QTextToSpeech::State state)
{
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(m_state);
}

void QTextToSpeechEngineAndroid::processNotifyReady()
{
    if (m_state != QTextToSpeech::Paused)
        setState(QTextToSpeech::Ready);
}

void QTextToSpeechEngineAndroid::processNotifyError()
{
    setState(QTextToSpeech::BackendError);
}

void QTextToSpeechEngineAndroid::processNotifySpeaking()
{
    setState(QTextToSpeech::Speaking);
}

void QTextToSpeechEngineAndroid::stop()
{
    if (m_state == QTextToSpeech::Ready)
        return;

    m_speech.callMethod<void>("stop", "()V");
    setState(QTextToSpeech::Ready);
}

void QTextToSpeechEngineAndroid::pause()
{
    if (m_state == QTextToSpeech::Paused)
        return;

    m_speech.callMethod<void>("stop", "()V");
    setState(QTextToSpeech::Paused);
}

void QTextToSpeechEngineAndroid::resume()
{
    if (m_state != QTextToSpeech::Paused)
        return;

    say(m_text);
}

double QTextToSpeechEngineAndroid::pitch() const
{
    jfloat pitch = m_speech.callMethod<jfloat>("pitch");
    return double(pitch - 1.0f);
}

bool QTextToSpeechEngineAndroid::setPitch(double pitch)
{
    // 0 == SUCCESS and 1.0 == Android API's normal pitch.
    return m_speech.callMethod<int>("setPitch", "(F)I", pitch + 1.0f) == 0;
}

double QTextToSpeechEngineAndroid::rate() const
{
    jfloat rate = m_speech.callMethod<jfloat>("rate");
    return double(rate - 1.0f);
}

bool QTextToSpeechEngineAndroid::setRate(double rate)
{
    // 0 == SUCCESS and 1.0 == Android API's normal rate.
    return (m_speech.callMethod<int>("setRate", "(F)I", rate + 1.0f) == 0);
}

double QTextToSpeechEngineAndroid::volume() const
{
    jfloat volume = m_speech.callMethod<jfloat>("volume");
    return volume;
}

bool QTextToSpeechEngineAndroid::setVolume(double volume)
{
    // 0 == SUCCESS
    return m_speech.callMethod<jint>("setVolume", "(F)I", float(volume)) == 0;
}

QVector<QLocale> QTextToSpeechEngineAndroid::availableLocales() const
{
    return QVector<QLocale>();
}

bool QTextToSpeechEngineAndroid::setLocale(const QLocale & /* locale */)
{
    return false;
}

QLocale QTextToSpeechEngineAndroid::locale() const
{
    return QLocale();
}

QVector<QVoice> QTextToSpeechEngineAndroid::availableVoices() const
{
    return QVector<QVoice>();
}

bool QTextToSpeechEngineAndroid::setVoice(const QVoice & /* voice */)
{
    return false;
}

QVoice QTextToSpeechEngineAndroid::voice() const
{
    return QVoice();
}

QT_END_NAMESPACE
