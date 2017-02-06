/****************************************************************************
**
** Copyright (C) 2016 Lauri Laanmets (Proekspert AS) <lauri.laanmets@eesti.ee>
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include <jni.h>
#include <android/log.h>
#include <QtCore/QLoggingCategory>
#include <QtBluetooth/qbluetoothglobal.h>
#include "android/jni_android_p.h"
#include "android/androidbroadcastreceiver_p.h"
#include "android/serveracceptancethread_p.h"
#include "android/inputstreamthread_p.h"
#include "android/lowenergynotificationhub_p.h"

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

typedef QHash<QByteArray, QAndroidJniObject> JCachedStringFields;
Q_GLOBAL_STATIC(JCachedStringFields, cachedStringFields)

//Java class names
static const char * const javaBluetoothAdapterClassName = "android/bluetooth/BluetoothAdapter";
static const char * const javaBluetoothDeviceClassName = "android/bluetooth/BluetoothDevice" ;

//Java field names
static const char * const javaActionAclConnected = "ACTION_ACL_CONNECTED";
static const char * const javaActionAclDisconnected = "ACTION_ACL_DISCONNECTED";
static const char * const javaActionBondStateChanged = "ACTION_BOND_STATE_CHANGED";
static const char * const javaActionDiscoveryStarted = "ACTION_DISCOVERY_STARTED";
static const char * const javaActionDiscoveryFinished = "ACTION_DISCOVERY_FINISHED";
static const char * const javaActionFound = "ACTION_FOUND";
static const char * const javaActionPairingRequest = "ACTION_PAIRING_REQUEST";
static const char * const javaActionScanModeChanged = "ACTION_SCAN_MODE_CHANGED";
static const char * const javaActionUuid = "ACTION_UUID";
static const char * const javaExtraBondState = "EXTRA_BOND_STATE";
static const char * const javaExtraDevice = "EXTRA_DEVICE";
static const char * const javaExtraPairingKey = "EXTRA_PAIRING_KEY";
static const char * const javaExtraPairingVariant = "EXTRA_PAIRING_VARIANT";
static const char * const javaExtraRssi = "EXTRA_RSSI";
static const char * const javaExtraScanMode = "EXTRA_SCAN_MODE";
static const char * const javaExtraUuid = "EXTRA_UUID";

/*
 * This function operates on the assumption that each
 * field is of type java/lang/String.
 */
QAndroidJniObject valueForStaticField(JavaNames javaName, JavaNames javaFieldName)
{
    //construct key
    //the switch statements are used to reduce the number of duplicated strings
    //in the library

    const char* className;
    switch (javaName) {
    case JavaNames::BluetoothAdapter:
        className = javaBluetoothAdapterClassName; break;
    case JavaNames::BluetoothDevice:
        className = javaBluetoothDeviceClassName; break;
    default:
        qCWarning(QT_BT_ANDROID) << "Unknown java class name passed to valueForStaticField():" << javaName;
        return QAndroidJniObject();
    }

    const char *fieldName;
    switch (javaFieldName) {
    case JavaNames::ActionAclConnected:
        fieldName = javaActionAclConnected; break;
    case JavaNames::ActionAclDisconnected:
        fieldName = javaActionAclDisconnected; break;
    case JavaNames::ActionBondStateChanged:
        fieldName = javaActionBondStateChanged; break;
    case JavaNames::ActionDiscoveryStarted:
        fieldName = javaActionDiscoveryStarted; break;
    case JavaNames::ActionDiscoveryFinished:
        fieldName = javaActionDiscoveryFinished; break;
    case JavaNames::ActionFound:
        fieldName = javaActionFound; break;
    case JavaNames::ActionPairingRequest:
        fieldName = javaActionPairingRequest; break;
    case JavaNames::ActionScanModeChanged:
        fieldName = javaActionScanModeChanged; break;
    case JavaNames::ActionUuid:
        fieldName = javaActionUuid; break;
    case JavaNames::ExtraBondState:
        fieldName = javaExtraBondState; break;
    case JavaNames::ExtraDevice:
        fieldName = javaExtraDevice; break;
    case JavaNames::ExtraPairingKey:
        fieldName = javaExtraPairingKey; break;
    case JavaNames::ExtraPairingVariant:
        fieldName = javaExtraPairingVariant; break;
    case JavaNames::ExtraRssi:
        fieldName = javaExtraRssi; break;
    case JavaNames::ExtraScanMode:
        fieldName = javaExtraScanMode; break;
    case JavaNames::ExtraUuid:
        fieldName = javaExtraUuid; break;
    default:
        qCWarning(QT_BT_ANDROID) << "Unknown java field name passed to valueForStaticField():" << javaFieldName;
        return QAndroidJniObject();
    }

    int offset_class = qstrlen(className);
    int offset_field = qstrlen(fieldName);
    QByteArray key(offset_class + offset_field, Qt::Uninitialized);
    memcpy(key.data(), className, offset_class);
    memcpy(key.data()+offset_class, fieldName, offset_field);

    JCachedStringFields::iterator it = cachedStringFields()->find(key);
    if (it == cachedStringFields()->end()) {
        QAndroidJniEnvironment env;
        QAndroidJniObject fieldValue = QAndroidJniObject::getStaticObjectField(
                                            className, fieldName, "Ljava/lang/String;");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            cachedStringFields()->insert(key, QAndroidJniObject());
            return QAndroidJniObject();
        }

        cachedStringFields()->insert(key, fieldValue);
        return fieldValue;
    } else {
        return it.value();
    }
}

void QtBroadcastReceiver_jniOnReceive(JNIEnv *env, jobject /*javaObject*/,
                                             jlong qtObject, jobject context, jobject intent)
{
    reinterpret_cast<AndroidBroadcastReceiver*>(qtObject)->onReceive(env, context, intent);
}

static void QtBluetoothSocketServer_errorOccurred(JNIEnv */*env*/, jobject /*javaObject*/,
                                           jlong qtObject, jint errorCode)
{
    reinterpret_cast<ServerAcceptanceThread*>(qtObject)->javaThreadErrorOccurred(errorCode);
}

static void QtBluetoothSocketServer_newSocket(JNIEnv */*env*/, jobject /*javaObject*/,
                                       jlong qtObject, jobject socket)
{
    reinterpret_cast<ServerAcceptanceThread*>(qtObject)->javaNewSocket(socket);
}

static void QtBluetoothInputStreamThread_errorOccurred(JNIEnv */*env*/, jobject /*javaObject*/,
                                           jlong qtObject, jint errorCode)
{
    reinterpret_cast<InputStreamThread*>(qtObject)->javaThreadErrorOccurred(errorCode);
}

static void QtBluetoothInputStreamThread_readyData(JNIEnv */*env*/, jobject /*javaObject*/,
                                       jlong qtObject, jbyteArray buffer, jint bufferLength)
{
    reinterpret_cast<InputStreamThread*>(qtObject)->javaReadyRead(buffer, bufferLength);
}

void QtBluetoothLE_leScanResult(JNIEnv *env, jobject, jlong qtObject, jobject bluetoothDevice,
                                jint rssi, jbyteArray scanRecord)
{
    reinterpret_cast<AndroidBroadcastReceiver*>(qtObject)->onReceiveLeScan(
                                                                env, bluetoothDevice, rssi,
                                                                scanRecord);
}


static JNINativeMethod methods[] = {
    {"jniOnReceive", "(JLandroid/content/Context;Landroid/content/Intent;)V",
                (void *) QtBroadcastReceiver_jniOnReceive},
};

static JNINativeMethod methods_le[] = {
    {"leScanResult", "(JLandroid/bluetooth/BluetoothDevice;I[B)V",
                (void *) QtBluetoothLE_leScanResult},
    {"leConnectionStateChange", "(JII)V",
                (void *) LowEnergyNotificationHub::lowEnergy_connectionChange},
    {"leServicesDiscovered", "(JILjava/lang/String;)V",
                (void *) LowEnergyNotificationHub::lowEnergy_servicesDiscovered},
    {"leServiceDetailDiscoveryFinished", "(JLjava/lang/String;II)V",
                (void *) LowEnergyNotificationHub::lowEnergy_serviceDetailsDiscovered},
    {"leCharacteristicRead", "(JLjava/lang/String;ILjava/lang/String;I[B)V",
                (void *) LowEnergyNotificationHub::lowEnergy_characteristicRead},
    {"leDescriptorRead", "(JLjava/lang/String;Ljava/lang/String;ILjava/lang/String;[B)V",
                (void *) LowEnergyNotificationHub::lowEnergy_descriptorRead},
    {"leCharacteristicWritten", "(JI[BI)V",
                (void *) LowEnergyNotificationHub::lowEnergy_characteristicWritten},
    {"leDescriptorWritten", "(JI[BI)V",
                (void *) LowEnergyNotificationHub::lowEnergy_descriptorWritten},
    {"leCharacteristicChanged", "(JI[B)V",
                (void *) LowEnergyNotificationHub::lowEnergy_characteristicChanged},
    {"leServiceError", "(JII)V",
                (void *) LowEnergyNotificationHub::lowEnergy_serviceError},
};

static JNINativeMethod methods_server[] = {
        {"errorOccurred", "(JI)V",
                    (void *) QtBluetoothSocketServer_errorOccurred},
        {"newSocket", "(JLandroid/bluetooth/BluetoothSocket;)V",
                    (void *) QtBluetoothSocketServer_newSocket},
};

static JNINativeMethod methods_inputStream[] = {
        {"errorOccurred", "(JI)V",
                    (void *) QtBluetoothInputStreamThread_errorOccurred},
        {"readyData", "(J[BI)V",
                    (void *) QtBluetoothInputStreamThread_readyData},
};

static const char logTag[] = "QtBluetooth";
static const char classErrorMsg[] = "Can't find class \"%s\"";

#define FIND_AND_CHECK_CLASS(CLASS_NAME) \
clazz = env->FindClass(CLASS_NAME); \
if (!clazz) { \
    __android_log_print(ANDROID_LOG_FATAL, logTag, classErrorMsg, CLASS_NAME); \
    return JNI_FALSE; \
}

static bool registerNatives(JNIEnv *env)
{
    jclass clazz;
    FIND_AND_CHECK_CLASS("org/qtproject/qt5/android/bluetooth/QtBluetoothBroadcastReceiver");

    if (env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(methods[0])) < 0) {
        __android_log_print(ANDROID_LOG_FATAL, logTag, "RegisterNatives for BroadcastReceiver failed");
        return false;
    }

    FIND_AND_CHECK_CLASS("org/qtproject/qt5/android/bluetooth/QtBluetoothLE");
    if (env->RegisterNatives(clazz, methods_le, sizeof(methods_le) / sizeof(methods_le[0])) < 0) {
        __android_log_print(ANDROID_LOG_FATAL, logTag, "RegisterNatives for QBLuetoothLE failed");
        return false;
    }


    FIND_AND_CHECK_CLASS("org/qtproject/qt5/android/bluetooth/QtBluetoothSocketServer");
    if (env->RegisterNatives(clazz, methods_server, sizeof(methods_server) / sizeof(methods_server[0])) < 0) {
        __android_log_print(ANDROID_LOG_FATAL, logTag, "RegisterNatives for SocketServer failed");
        return false;
    }

    FIND_AND_CHECK_CLASS("org/qtproject/qt5/android/bluetooth/QtBluetoothInputStreamThread");
    if (env->RegisterNatives(clazz, methods_inputStream,
                             sizeof(methods_inputStream) / sizeof(methods_inputStream[0])) < 0) {
        __android_log_print(ANDROID_LOG_FATAL, logTag, "RegisterNatives for InputStreamThread failed");
        return false;
    }

    return true;
}

Q_BLUETOOTH_EXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/)
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
    uenv.venv = 0;

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK) {
        __android_log_print(ANDROID_LOG_FATAL, logTag, "GetEnv failed");
        return -1;
    }

    JNIEnv *env = uenv.nativeEnvironment;
    if (!registerNatives(env)) {
        __android_log_print(ANDROID_LOG_FATAL, logTag, "registerNatives failed");
        return -1;
    }

    if (QT_BT_ANDROID().isDebugEnabled())
        __android_log_print(ANDROID_LOG_INFO, logTag, "Bluetooth start");

    return JNI_VERSION_1_4;
}
