/****************************************************************************
**
** Copyright (C) 2016 BogDan Vatra <bogdan@kde.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include <qglobal.h>
#include <android/log.h>
#include <QtCore/QReadWriteLock>
#include <QtCore/QHash>

#include "androidjnisensors.h"

static JavaVM *javaVM = 0;
jclass sensorsClass;

static jmethodID getSensorListMethodId;
static jmethodID registerSensorMethodId;
static jmethodID unregisterSensorMethodId;
static jmethodID getSensorDescriptionMethodId;
static jmethodID getSensorMaximumRangeMethodId;
static jmethodID getCompassAzimuthId;

static QHash<int, QList<AndroidSensors::AndroidSensorsListenerInterface *> > listenersHash;
QReadWriteLock listenersLocker;
enum {
    SENSOR_DELAY_FASTEST = 0,
    SENSOR_DELAY_GAME = 1,
    SENSOR_DELAY_NORMAL = 3,
    SENSOR_DELAY_UI =2
};
namespace AndroidSensors
{
    struct AttachedJNIEnv
    {
        AttachedJNIEnv()
        {
            attached = false;
            if (javaVM->GetEnv((void**)&jniEnv, JNI_VERSION_1_6) < 0) {
                if (javaVM->AttachCurrentThread(&jniEnv, NULL) < 0) {
                    __android_log_print(ANDROID_LOG_ERROR, "Qt", "AttachCurrentThread failed");
                    jniEnv = 0;
                    return;
                }
                attached = true;
            }
        }

        ~AttachedJNIEnv()
        {
            if (attached)
                javaVM->DetachCurrentThread();
        }
        bool attached;
        JNIEnv *jniEnv;
    };

    QVector<AndroidSensorType> availableSensors()
    {
        QVector<AndroidSensorType> ret;
        AttachedJNIEnv aenv;
        if (!aenv.jniEnv)
            return ret;
        jintArray jsensors = static_cast<jintArray>(aenv.jniEnv->CallStaticObjectMethod(sensorsClass,
                                                                                        getSensorListMethodId));
        jint *sensors = aenv.jniEnv->GetIntArrayElements(jsensors, 0);
        const uint sz = aenv.jniEnv->GetArrayLength(jsensors);
        for (uint i = 0; i < sz; i++)
            ret.push_back(AndroidSensorType(sensors[i]));
        aenv.jniEnv->ReleaseIntArrayElements(jsensors, sensors, 0);
        return ret;
    }

    QString sensorDescription(AndroidSensorType sensor)
    {
        AttachedJNIEnv aenv;
        if (!aenv.jniEnv)
            return QString();
        jstring jstr = static_cast<jstring>(aenv.jniEnv->CallStaticObjectMethod(sensorsClass,
                                                                                getSensorDescriptionMethodId,
                                                                                jint(sensor)));
        if (!jstr)
            return QString();
        const jchar *pstr = aenv.jniEnv->GetStringChars(jstr, 0);
        QString ret(reinterpret_cast<const QChar *>(pstr), aenv.jniEnv->GetStringLength(jstr));
        aenv.jniEnv->ReleaseStringChars(jstr, pstr);
        aenv.jniEnv->DeleteLocalRef(jstr);
        return ret;
    }

    qreal sensorMaximumRange(AndroidSensorType sensor)
    {
        AttachedJNIEnv aenv;
        if (!aenv.jniEnv)
            return 0;
        jfloat range = aenv.jniEnv->CallStaticFloatMethod(sensorsClass, getSensorMaximumRangeMethodId, jint(sensor));
        return range;
    }

    bool registerListener(AndroidSensorType sensor, AndroidSensorsListenerInterface *listener, int dataRate)
    {
        listenersLocker.lockForWrite();
        bool startService = listenersHash[sensor].empty();
        listenersHash[sensor].push_back(listener);
        listenersLocker.unlock();
        if (startService) {
            AttachedJNIEnv aenv;
            if (!aenv.jniEnv)
                return false;
            int rate = dataRate > 0 ? 1000000/dataRate : SENSOR_DELAY_GAME;
            return aenv.jniEnv->CallStaticBooleanMethod(sensorsClass,
                                                        registerSensorMethodId,
                                                        jint(sensor),
                                                        jint(rate));
        }
        return true;
    }

    bool unregisterListener(AndroidSensorType sensor, AndroidSensorsListenerInterface *listener)
    {
        listenersLocker.lockForWrite();
        listenersHash[sensor].removeOne(listener);
        bool stopService = listenersHash[sensor].empty();
        if (stopService)
            listenersHash.remove(sensor);
        listenersLocker.unlock();
        if (stopService) {
            AttachedJNIEnv aenv;
            if (!aenv.jniEnv)
                return false;
            return aenv.jniEnv->CallStaticBooleanMethod(sensorsClass, unregisterSensorMethodId, jint(sensor));
        }
        return true;
    }

    qreal getCompassAzimuth(jfloat *accelerometerReading, jfloat *magnetometerReading)
    {
            AttachedJNIEnv aenv;
            if (!aenv.jniEnv)
                return 0.0;
            return aenv.jniEnv->CallStaticFloatMethod(sensorsClass, getCompassAzimuthId,
                                                        accelerometerReading[0],
                                                        accelerometerReading[1],
                                                        accelerometerReading[2],
                                                        magnetometerReading[0],
                                                        magnetometerReading[1],
                                                        magnetometerReading[2]);
    }
}

static const char logTag[] = "Qt";
static const char classErrorMsg[] = "Can't find class \"%s\"";
static const char methodErrorMsg[] = "Can't find method \"%s%s\"";

static void accuracyChanged(JNIEnv * /*env*/, jobject /*thiz*/, jint sensor, jint accuracy)
{
    listenersLocker.lockForRead();
    foreach (AndroidSensors::AndroidSensorsListenerInterface *listener, listenersHash[sensor])
        listener->onAccuracyChanged(accuracy);
    listenersLocker.unlock();
}

static void sensorChanged(JNIEnv *env, jobject /*thiz*/, jint sensor, jlong timeStamp, jfloatArray array)
{
    uint size = env->GetArrayLength(array);
    jfloat *values = env->GetFloatArrayElements(array, 0);
    listenersLocker.lockForRead();
    foreach (AndroidSensors::AndroidSensorsListenerInterface *listener, listenersHash[sensor])
        listener->onSensorChanged(timeStamp, values, size);
    listenersLocker.unlock();
    env->ReleaseFloatArrayElements(array, values, JNI_ABORT); // don't copy back the elements

}

static JNINativeMethod methods[] = {
    {"accuracyChanged", "(II)V", (void *)accuracyChanged},
    {"sensorChanged", "(IJ[F)V", (void *)sensorChanged}
};

#define FIND_AND_CHECK_CLASS(CLASS_NAME) \
clazz = env->FindClass(CLASS_NAME); \
if (!clazz) { \
    __android_log_print(ANDROID_LOG_FATAL, logTag, classErrorMsg, CLASS_NAME); \
    return JNI_FALSE; \
}

#define GET_AND_CHECK_STATIC_METHOD(VAR, CLASS, METHOD_NAME, METHOD_SIGNATURE) \
VAR = env->GetStaticMethodID(CLASS, METHOD_NAME, METHOD_SIGNATURE); \
if (!VAR) { \
    __android_log_print(ANDROID_LOG_FATAL, logTag, methodErrorMsg, METHOD_NAME, METHOD_SIGNATURE); \
    return JNI_FALSE; \
}

static bool registerNatives(JNIEnv *env)
{
    jclass clazz;
    FIND_AND_CHECK_CLASS("org/qtproject/qt5/android/sensors/QtSensors");
    sensorsClass = static_cast<jclass>(env->NewGlobalRef(clazz));

    if (env->RegisterNatives(sensorsClass, methods, sizeof(methods) / sizeof(methods[0])) < 0) {
        __android_log_print(ANDROID_LOG_FATAL, logTag, "RegisterNatives failed");
        return JNI_FALSE;
    }

    GET_AND_CHECK_STATIC_METHOD(getSensorListMethodId, sensorsClass, "getSensorList", "()[I");
    GET_AND_CHECK_STATIC_METHOD(registerSensorMethodId, sensorsClass, "registerSensor", "(II)Z");
    GET_AND_CHECK_STATIC_METHOD(unregisterSensorMethodId, sensorsClass, "unregisterSensor", "(I)Z");
    GET_AND_CHECK_STATIC_METHOD(getSensorDescriptionMethodId, sensorsClass, "getSensorDescription", "(I)Ljava/lang/String;");
    GET_AND_CHECK_STATIC_METHOD(getSensorMaximumRangeMethodId, sensorsClass, "getSensorMaximumRange", "(I)F");
    GET_AND_CHECK_STATIC_METHOD(getCompassAzimuthId, sensorsClass, "getCompassAzimuth", "(FFFFFF)F");

    return true;
}

Q_DECL_EXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void * /*reserved*/)
{
    static bool initialized = false;
    if (initialized)
        return JNI_VERSION_1_6;
    initialized = true;

    typedef union {
        JNIEnv *nativeEnvironment;
        void *venv;
    } UnionJNIEnvToVoid;

    __android_log_print(ANDROID_LOG_INFO, logTag, "Sensors start");
    UnionJNIEnvToVoid uenv;
    uenv.venv = NULL;
    javaVM = 0;

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK) {
        __android_log_print(ANDROID_LOG_FATAL, logTag, "GetEnv failed");
        return -1;
    }
    JNIEnv *env = uenv.nativeEnvironment;
    if (!registerNatives(env)) {
        __android_log_print(ANDROID_LOG_FATAL, logTag, "registerNatives failed");
        return -1;
    }

    javaVM = vm;
    return JNI_VERSION_1_4;
}
