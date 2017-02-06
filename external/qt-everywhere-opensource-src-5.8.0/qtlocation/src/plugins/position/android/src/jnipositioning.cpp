/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include <QDateTime>
#include <QDebug>
#include <QMap>
#include <QtGlobal>
#include <QtCore/private/qjnihelpers_p.h>
#include <android/log.h>
#include <jni.h>
#include <QGeoPositionInfo>
#include "qgeopositioninfosource_android_p.h"
#include "qgeosatelliteinfosource_android_p.h"

#include "jnipositioning.h"

static JavaVM *javaVM = 0;
jclass positioningClass;

static jmethodID providerListMethodId;
static jmethodID lastKnownPositionMethodId;
static jmethodID startUpdatesMethodId;
static jmethodID stopUpdatesMethodId;
static jmethodID requestUpdateMethodId;
static jmethodID startSatelliteUpdatesMethodId;

static const char logTag[] = "QtPositioning";
static const char classErrorMsg[] = "Can't find class \"%s\"";
static const char methodErrorMsg[] = "Can't find method \"%s%s\"";

namespace AndroidPositioning {
    typedef QMap<int, QGeoPositionInfoSourceAndroid * > PositionSourceMap;
    typedef QMap<int, QGeoSatelliteInfoSourceAndroid * > SatelliteSourceMap;

    Q_GLOBAL_STATIC(PositionSourceMap, idToPosSource)

    Q_GLOBAL_STATIC(SatelliteSourceMap, idToSatSource)

    struct AttachedJNIEnv
    {
        AttachedJNIEnv()
        {
            attached = false;
            if (javaVM->GetEnv((void**)&jniEnv, JNI_VERSION_1_6) < 0) {
                if (javaVM->AttachCurrentThread(&jniEnv, NULL) < 0) {
                    __android_log_print(ANDROID_LOG_ERROR, logTag, "AttachCurrentThread failed");
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

    int registerPositionInfoSource(QObject *obj)
    {
        static bool firstInit = true;
        if (firstInit) {
            qsrand( QDateTime::currentMSecsSinceEpoch() / 1000 );
            firstInit = false;
        }

        int key = -1;
        if (obj->inherits("QGeoPositionInfoSource")) {
            QGeoPositionInfoSourceAndroid *src = qobject_cast<QGeoPositionInfoSourceAndroid *>(obj);
            Q_ASSERT(src);
            do {
                key = qrand();
            } while (idToPosSource()->contains(key));

            idToPosSource()->insert(key, src);
        } else if (obj->inherits("QGeoSatelliteInfoSource")) {
            QGeoSatelliteInfoSourceAndroid *src = qobject_cast<QGeoSatelliteInfoSourceAndroid *>(obj);
            Q_ASSERT(src);
            do {
                key = qrand();
            } while (idToSatSource()->contains(key));

            idToSatSource()->insert(key, src);
        }

        return key;
    }

    void unregisterPositionInfoSource(int key)
    {
        idToPosSource()->remove(key);
        idToSatSource()->remove(key);
    }

    enum PositionProvider
    {
        PROVIDER_GPS = 0,
        PROVIDER_NETWORK = 1,
        PROVIDER_PASSIVE = 2
    };


    QGeoPositionInfoSource::PositioningMethods availableProviders()
    {
        QGeoPositionInfoSource::PositioningMethods ret =
                static_cast<QGeoPositionInfoSource::PositioningMethods>(0);
        AttachedJNIEnv env;
        if (!env.jniEnv)
            return ret;
        jintArray jProviders = static_cast<jintArray>(env.jniEnv->CallStaticObjectMethod(
                                                          positioningClass, providerListMethodId));
        jint *providers = env.jniEnv->GetIntArrayElements(jProviders, 0);
        const uint size = env.jniEnv->GetArrayLength(jProviders);
        for (uint i = 0; i < size; i++) {
            switch (providers[i]) {
            case PROVIDER_GPS:
                ret |= QGeoPositionInfoSource::SatellitePositioningMethods;
                break;
            case PROVIDER_NETWORK:
                ret |= QGeoPositionInfoSource::NonSatellitePositioningMethods;
                break;
            case PROVIDER_PASSIVE:
                //we ignore as Qt doesn't have interface for it right now
                break;
            default:
                __android_log_print(ANDROID_LOG_INFO, logTag, "Unknown positioningMethod");
            }
        }

        env.jniEnv->ReleaseIntArrayElements(jProviders, providers, 0);
        env.jniEnv->DeleteLocalRef(jProviders);

        return ret;
    }

    //caching originally taken from corelib/kernel/qjni.cpp
    typedef QHash<QByteArray, jmethodID> JMethodIDHash;
    Q_GLOBAL_STATIC(JMethodIDHash, cachedMethodID)

    static jmethodID getCachedMethodID(JNIEnv *env,
                                       jclass clazz,
                                       const char *name,
                                       const char *sig)
    {
        jmethodID id = 0;
        int offset_name = qstrlen(name);
        int offset_signal = qstrlen(sig);
        QByteArray key(offset_name + offset_signal, Qt::Uninitialized);
        memcpy(key.data(), name, offset_name);
        memcpy(key.data()+offset_name, sig, offset_signal);
        QHash<QByteArray, jmethodID>::iterator it = cachedMethodID->find(key);
        if (it == cachedMethodID->end()) {
            id = env->GetMethodID(clazz, name, sig);
            if (env->ExceptionCheck()) {
                id = 0;
    #ifdef QT_DEBUG
                env->ExceptionDescribe();
    #endif // QT_DEBUG
                env->ExceptionClear();
            }

            cachedMethodID->insert(key, id);
        } else {
            id = it.value();
        }
        return id;
    }

    QGeoPositionInfo positionInfoFromJavaLocation(JNIEnv * jniEnv, const jobject &location)
    {
        QGeoPositionInfo info;
        jclass thisClass = jniEnv->GetObjectClass(location);
        if (!thisClass)
            return QGeoPositionInfo();

        jmethodID mid = getCachedMethodID(jniEnv, thisClass, "getLatitude", "()D");
        jdouble latitude = jniEnv->CallDoubleMethod(location, mid);
        mid = getCachedMethodID(jniEnv, thisClass, "getLongitude", "()D");
        jdouble longitude = jniEnv->CallDoubleMethod(location, mid);
        QGeoCoordinate coordinate(latitude, longitude);

        //altitude
        mid = getCachedMethodID(jniEnv, thisClass, "hasAltitude", "()Z");
        jboolean attributeExists = jniEnv->CallBooleanMethod(location, mid);
        if (attributeExists) {
            mid = getCachedMethodID(jniEnv, thisClass, "getAltitude", "()D");
            jdouble value = jniEnv->CallDoubleMethod(location, mid);
            coordinate.setAltitude(value);
        }

        info.setCoordinate(coordinate);

        //time stamp
        mid = getCachedMethodID(jniEnv, thisClass, "getTime", "()J");
        jlong timestamp = jniEnv->CallLongMethod(location, mid);
        info.setTimestamp(QDateTime::fromMSecsSinceEpoch(timestamp));

        //accuracy
        mid = getCachedMethodID(jniEnv, thisClass, "hasAccuracy", "()Z");
        attributeExists = jniEnv->CallBooleanMethod(location, mid);
        if (attributeExists) {
            mid = getCachedMethodID(jniEnv, thisClass, "getAccuracy", "()F");
            jfloat accuracy = jniEnv->CallFloatMethod(location, mid);
            info.setAttribute(QGeoPositionInfo::HorizontalAccuracy, accuracy);
        }

        //ground speed
        mid = getCachedMethodID(jniEnv, thisClass, "hasSpeed", "()Z");
        attributeExists = jniEnv->CallBooleanMethod(location, mid);
        if (attributeExists) {
            mid = getCachedMethodID(jniEnv, thisClass, "getSpeed", "()F");
            jfloat speed = jniEnv->CallFloatMethod(location, mid);
            info.setAttribute(QGeoPositionInfo::GroundSpeed, speed);
        }

        //bearing
        mid = getCachedMethodID(jniEnv, thisClass, "hasBearing", "()Z");
        attributeExists = jniEnv->CallBooleanMethod(location, mid);
        if (attributeExists) {
            mid = getCachedMethodID(jniEnv, thisClass, "getBearing", "()F");
            jfloat bearing = jniEnv->CallFloatMethod(location, mid);
            info.setAttribute(QGeoPositionInfo::Direction, bearing);
        }

        jniEnv->DeleteLocalRef(thisClass);
        return info;
    }

    QList<QGeoSatelliteInfo> satelliteInfoFromJavaLocation(JNIEnv *jniEnv,
                                                           jobjectArray satellites,
                                                           QList<QGeoSatelliteInfo>* usedInFix)
    {
        QList<QGeoSatelliteInfo> sats;
        jsize length = jniEnv->GetArrayLength(satellites);
        for (int i = 0; i<length; i++) {
            jobject element = jniEnv->GetObjectArrayElement(satellites, i);
            if (jniEnv->ExceptionOccurred()) {
                qWarning() << "Cannot process all satellite data due to exception.";
                break;
            }

            jclass thisClass = jniEnv->GetObjectClass(element);
            if (!thisClass)
                continue;

            QGeoSatelliteInfo info;

            //signal strength
            jmethodID mid = getCachedMethodID(jniEnv, thisClass, "getSnr", "()F");
            jfloat snr = jniEnv->CallFloatMethod(element, mid);
            info.setSignalStrength((int)snr);

            //ignore any satellite with no signal whatsoever
            if (qFuzzyIsNull(snr))
                continue;

            //prn
            mid = getCachedMethodID(jniEnv, thisClass, "getPrn", "()I");
            jint prn = jniEnv->CallIntMethod(element, mid);
            info.setSatelliteIdentifier(prn);

            if (prn >= 1 && prn <= 32)
                info.setSatelliteSystem(QGeoSatelliteInfo::GPS);
            else if (prn >= 65 && prn <= 96)
                info.setSatelliteSystem(QGeoSatelliteInfo::GLONASS);

            //azimuth
            mid = getCachedMethodID(jniEnv, thisClass, "getAzimuth", "()F");
            jfloat azimuth = jniEnv->CallFloatMethod(element, mid);
            info.setAttribute(QGeoSatelliteInfo::Azimuth, azimuth);

            //elevation
            mid = getCachedMethodID(jniEnv, thisClass, "getElevation", "()F");
            jfloat elevation = jniEnv->CallFloatMethod(element, mid);
            info.setAttribute(QGeoSatelliteInfo::Elevation, elevation);

            //used in a fix
            mid = getCachedMethodID(jniEnv, thisClass, "usedInFix", "()Z");
            jboolean inFix = jniEnv->CallBooleanMethod(element, mid);

            sats.append(info);

            if (inFix)
                usedInFix->append(info);

            jniEnv->DeleteLocalRef(thisClass);
            jniEnv->DeleteLocalRef(element);
        }

        return sats;
    }

    QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly)
    {
        AttachedJNIEnv env;
        if (!env.jniEnv)
            return QGeoPositionInfo();

        jobject location = env.jniEnv->CallStaticObjectMethod(positioningClass,
                                                              lastKnownPositionMethodId,
                                                              fromSatellitePositioningMethodsOnly);
        if (location == 0)
            return QGeoPositionInfo();

        const QGeoPositionInfo info = positionInfoFromJavaLocation(env.jniEnv, location);
        env.jniEnv->DeleteLocalRef(location);

        return info;
    }

    inline int positioningMethodToInt(QGeoPositionInfoSource::PositioningMethods m)
    {
        int providerSelection = 0;
        if (m & QGeoPositionInfoSource::SatellitePositioningMethods)
            providerSelection |= 1;
        if (m & QGeoPositionInfoSource::NonSatellitePositioningMethods)
            providerSelection |= 2;

        return providerSelection;
    }

    QGeoPositionInfoSource::Error startUpdates(int androidClassKey)
    {
        AttachedJNIEnv env;
        if (!env.jniEnv)
            return QGeoPositionInfoSource::UnknownSourceError;

        QGeoPositionInfoSourceAndroid *source = AndroidPositioning::idToPosSource()->value(androidClassKey);

        if (source) {
            // Android v23+ requires runtime permission check and requests
            QString permission(QLatin1String("android.permission.ACCESS_FINE_LOCATION"));

            if (QtAndroidPrivate::checkPermission(permission) == QtAndroidPrivate::PermissionsResult::Denied) {
                const QHash<QString, QtAndroidPrivate::PermissionsResult> results =
                        QtAndroidPrivate::requestPermissionsSync(env.jniEnv, QStringList() << permission);
                if (!results.contains(permission)
                    || results[permission] == QtAndroidPrivate::PermissionsResult::Denied)
                {
                    qWarning() << "Position retrieval not possible due to missing permission (ACCESS_FINE_LOCATION)";
                    return QGeoPositionInfoSource::AccessError;
                }
            }

            int errorCode = env.jniEnv->CallStaticIntMethod(positioningClass, startUpdatesMethodId,
                                             androidClassKey,
                                             positioningMethodToInt(source->preferredPositioningMethods()),
                                             source->updateInterval());
            switch (errorCode) {
            case 0:
            case 1:
            case 2:
            case 3:
                return static_cast<QGeoPositionInfoSource::Error>(errorCode);
            default:
                break;
            }
        }

        return QGeoPositionInfoSource::UnknownSourceError;
    }

    //used for stopping regular and single updates
    void stopUpdates(int androidClassKey)
    {
        AttachedJNIEnv env;
        if (!env.jniEnv)
            return;

        env.jniEnv->CallStaticVoidMethod(positioningClass, stopUpdatesMethodId, androidClassKey);
    }

    QGeoPositionInfoSource::Error requestUpdate(int androidClassKey)
    {
        AttachedJNIEnv env;
        if (!env.jniEnv)
            return QGeoPositionInfoSource::UnknownSourceError;

        QGeoPositionInfoSourceAndroid *source = AndroidPositioning::idToPosSource()->value(androidClassKey);

        if (source) {
            // Android v23+ requires runtime permission check and requests
            QString permission(QLatin1String("android.permission.ACCESS_FINE_LOCATION"));

            if (QtAndroidPrivate::checkPermission(permission) == QtAndroidPrivate::PermissionsResult::Denied) {
                const QHash<QString, QtAndroidPrivate::PermissionsResult> results =
                        QtAndroidPrivate::requestPermissionsSync(env.jniEnv, QStringList() << permission);
                if (!results.contains(permission)
                    || results[permission] == QtAndroidPrivate::PermissionsResult::Denied)
                {
                    qWarning() << "Position update not possible due to missing permission (ACCESS_FINE_LOCATION)";
                    return QGeoPositionInfoSource::AccessError;
                }
            }

            int errorCode = env.jniEnv->CallStaticIntMethod(positioningClass, requestUpdateMethodId,
                                             androidClassKey,
                                             positioningMethodToInt(source->preferredPositioningMethods()));
            switch (errorCode) {
            case 0:
            case 1:
            case 2:
            case 3:
                return static_cast<QGeoPositionInfoSource::Error>(errorCode);
            default:
                break;
            }
        }
        return QGeoPositionInfoSource::UnknownSourceError;
    }

    QGeoSatelliteInfoSource::Error startSatelliteUpdates(int androidClassKey, bool isSingleRequest, int requestTimeout)
    {
        AttachedJNIEnv env;
        if (!env.jniEnv)
            return QGeoSatelliteInfoSource::UnknownSourceError;

        QGeoSatelliteInfoSourceAndroid *source = AndroidPositioning::idToSatSource()->value(androidClassKey);

        if (source) {
            int interval = source->updateInterval();
            if (isSingleRequest)
                interval = requestTimeout;
            int errorCode = env.jniEnv->CallStaticIntMethod(positioningClass, startSatelliteUpdatesMethodId,
                                             androidClassKey,
                                             interval, isSingleRequest);
            switch (errorCode) {
            case -1:
            case 0:
            case 1:
            case 2:
                return static_cast<QGeoSatelliteInfoSource::Error>(errorCode);
            default:
                qWarning() << "startSatelliteUpdates: Unknown error code " << errorCode;
                break;
            }
        }
        return QGeoSatelliteInfoSource::UnknownSourceError;
    }
}


static void positionUpdated(JNIEnv *env, jobject /*thiz*/, jobject location, jint androidClassKey, jboolean isSingleUpdate)
{
    QGeoPositionInfo info = AndroidPositioning::positionInfoFromJavaLocation(env, location);

    QGeoPositionInfoSourceAndroid *source = AndroidPositioning::idToPosSource()->value(androidClassKey);
    if (!source) {
        qWarning("positionUpdated: source == 0");
        return;
    }

    //we need to invoke indirectly as the Looper thread is likely to be not the same thread
    if (!isSingleUpdate)
        QMetaObject::invokeMethod(source, "processPositionUpdate", Qt::AutoConnection,
                              Q_ARG(QGeoPositionInfo, info));
    else
        QMetaObject::invokeMethod(source, "processSinglePositionUpdate", Qt::AutoConnection,
                              Q_ARG(QGeoPositionInfo, info));
}

static void locationProvidersDisabled(JNIEnv *env, jobject /*thiz*/, jint androidClassKey)
{
    Q_UNUSED(env);
    QObject *source = AndroidPositioning::idToPosSource()->value(androidClassKey);
    if (!source)
        source = AndroidPositioning::idToSatSource()->value(androidClassKey);
    if (!source) {
        qWarning("locationProvidersDisabled: source == 0");
        return;
    }

    QMetaObject::invokeMethod(source, "locationProviderDisabled", Qt::AutoConnection);
}

static void satelliteUpdated(JNIEnv *env, jobject /*thiz*/, jobjectArray satellites, jint androidClassKey, jboolean isSingleUpdate)
{
    QList<QGeoSatelliteInfo> inUse;
    QList<QGeoSatelliteInfo> sats = AndroidPositioning::satelliteInfoFromJavaLocation(env, satellites, &inUse);

    QGeoSatelliteInfoSourceAndroid *source = AndroidPositioning::idToSatSource()->value(androidClassKey);
    if (!source) {
        qFatal("satelliteUpdated: source == 0");
        return;
    }

    QMetaObject::invokeMethod(source, "processSatelliteUpdateInView", Qt::AutoConnection,
                              Q_ARG(QList<QGeoSatelliteInfo>, sats), Q_ARG(bool, isSingleUpdate));

    QMetaObject::invokeMethod(source, "processSatelliteUpdateInUse", Qt::AutoConnection,
                              Q_ARG(QList<QGeoSatelliteInfo>, inUse), Q_ARG(bool, isSingleUpdate));
}


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

static JNINativeMethod methods[] = {
    {"positionUpdated", "(Landroid/location/Location;IZ)V", (void *)positionUpdated},
    {"locationProvidersDisabled", "(I)V", (void *) locationProvidersDisabled},
    {"satelliteUpdated", "([Landroid/location/GpsSatellite;IZ)V", (void *)satelliteUpdated}
};

static bool registerNatives(JNIEnv *env)
{
    jclass clazz;
    FIND_AND_CHECK_CLASS("org/qtproject/qt5/android/positioning/QtPositioning");
    positioningClass = static_cast<jclass>(env->NewGlobalRef(clazz));

    if (env->RegisterNatives(positioningClass, methods, sizeof(methods) / sizeof(methods[0])) < 0) {
        __android_log_print(ANDROID_LOG_FATAL, logTag, "RegisterNatives failed");
        return JNI_FALSE;
    }

    GET_AND_CHECK_STATIC_METHOD(providerListMethodId, positioningClass, "providerList", "()[I");
    GET_AND_CHECK_STATIC_METHOD(lastKnownPositionMethodId, positioningClass, "lastKnownPosition", "(Z)Landroid/location/Location;");
    GET_AND_CHECK_STATIC_METHOD(startUpdatesMethodId, positioningClass, "startUpdates", "(III)I");
    GET_AND_CHECK_STATIC_METHOD(stopUpdatesMethodId, positioningClass, "stopUpdates", "(I)V");
    GET_AND_CHECK_STATIC_METHOD(requestUpdateMethodId, positioningClass, "requestUpdate", "(II)I");
    GET_AND_CHECK_STATIC_METHOD(startSatelliteUpdatesMethodId, positioningClass, "startSatelliteUpdates", "(IIZ)I");

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

    __android_log_print(ANDROID_LOG_INFO, logTag, "Positioning start");
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

