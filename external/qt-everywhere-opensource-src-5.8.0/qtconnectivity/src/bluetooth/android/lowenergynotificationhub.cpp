/****************************************************************************
**
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

#include "lowenergynotificationhub_p.h"

#include <QtCore/QHash>
#include <QtCore/QLoggingCategory>
#include <QtCore/QTime>
#include <QtAndroidExtras/QAndroidJniEnvironment>

QT_BEGIN_NAMESPACE

typedef QHash<long, LowEnergyNotificationHub*> HubMapType;
Q_GLOBAL_STATIC(HubMapType, hubMap)

QReadWriteLock LowEnergyNotificationHub::lock;

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

LowEnergyNotificationHub::LowEnergyNotificationHub(
        const QBluetoothAddress &remote, QObject *parent)
    :   QObject(parent), javaToCtoken(0)
{
    QAndroidJniEnvironment env;
    const QAndroidJniObject address =
            QAndroidJniObject::fromString(remote.toString());
    jBluetoothLe = QAndroidJniObject("org/qtproject/qt5/android/bluetooth/QtBluetoothLE",
                                     "(Ljava/lang/String;Landroid/content/Context;)V",
                                     address.object<jstring>(),
                                     QtAndroidPrivate::activity() ? QtAndroidPrivate::activity() : QtAndroidPrivate::service());


    if (env->ExceptionCheck() || !jBluetoothLe.isValid()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        jBluetoothLe = QAndroidJniObject();
        return;
    }

    // register C++ class pointer in Java
    qsrand(QTime::currentTime().msec());
    lock.lockForWrite();

    while (true) {
        javaToCtoken = qrand();
        if (!hubMap()->contains(javaToCtoken))
            break;
    }

    hubMap()->insert(javaToCtoken, this);
    lock.unlock();

    jBluetoothLe.setField<jlong>("qtObject", javaToCtoken);
}

LowEnergyNotificationHub::~LowEnergyNotificationHub()
{
    lock.lockForWrite();
    hubMap()->remove(javaToCtoken);
    lock.unlock();
}

// runs in Java thread
void LowEnergyNotificationHub::lowEnergy_connectionChange(JNIEnv *, jobject, jlong qtObject, jint errorCode, jint newState)
{
    lock.lockForRead();
    LowEnergyNotificationHub *hub = hubMap()->value(qtObject);
    lock.unlock();
    if (!hub)
        return;

    QMetaObject::invokeMethod(hub, "connectionUpdated", Qt::QueuedConnection,
                              Q_ARG(QLowEnergyController::ControllerState,
                                    (QLowEnergyController::ControllerState)newState),
                              Q_ARG(QLowEnergyController::Error,
                                    (QLowEnergyController::Error)errorCode));
}

void LowEnergyNotificationHub::lowEnergy_servicesDiscovered(
        JNIEnv *, jobject, jlong qtObject, jint errorCode, jobject uuidList)
{
    lock.lockForRead();
    LowEnergyNotificationHub *hub = hubMap()->value(qtObject);
    lock.unlock();
    if (!hub)
        return;

    const QString uuids = QAndroidJniObject(uuidList).toString();
    QMetaObject::invokeMethod(hub, "servicesDiscovered", Qt::QueuedConnection,
                              Q_ARG(QLowEnergyController::Error,
                                    (QLowEnergyController::Error)errorCode),
                              Q_ARG(QString, uuids));
}

void LowEnergyNotificationHub::lowEnergy_serviceDetailsDiscovered(
        JNIEnv *, jobject, jlong qtObject, jobject uuid, jint startHandle,
        jint endHandle)
{
    lock.lockForRead();
    LowEnergyNotificationHub *hub = hubMap()->value(qtObject);
    lock.unlock();
    if (!hub)
        return;

    const QString serviceUuid = QAndroidJniObject(uuid).toString();
    QMetaObject::invokeMethod(hub, "serviceDetailsDiscoveryFinished",
                              Qt::QueuedConnection,
                              Q_ARG(QString, serviceUuid),
                              Q_ARG(int, startHandle),
                              Q_ARG(int, endHandle));
}

void LowEnergyNotificationHub::lowEnergy_characteristicRead(
        JNIEnv *env, jobject, jlong qtObject, jobject sUuid, jint handle,
        jobject cUuid, jint properties, jbyteArray data)
{
    lock.lockForRead();
    LowEnergyNotificationHub *hub = hubMap()->value(qtObject);
    lock.unlock();
    if (!hub)
        return;

    const QBluetoothUuid serviceUuid(QAndroidJniObject(sUuid).toString());
    if (serviceUuid.isNull())
        return;

    const QBluetoothUuid charUuid(QAndroidJniObject(cUuid).toString());
    if (charUuid.isNull())
        return;

    QByteArray payload;
    if (data) { //empty Java byte array is 0x0
        jsize length = env->GetArrayLength(data);
        payload.resize(length);
        env->GetByteArrayRegion(data, 0, length,
                                reinterpret_cast<signed char*>(payload.data()));
    }

    QMetaObject::invokeMethod(hub, "characteristicRead", Qt::QueuedConnection,
                              Q_ARG(QBluetoothUuid, serviceUuid),
                              Q_ARG(int, handle),
                              Q_ARG(QBluetoothUuid, charUuid),
                              Q_ARG(int, properties),
                              Q_ARG(QByteArray, payload));

}

void LowEnergyNotificationHub::lowEnergy_descriptorRead(
        JNIEnv *env, jobject, jlong qtObject, jobject sUuid, jobject cUuid,
        jint handle, jobject dUuid, jbyteArray data)
{
    lock.lockForRead();
    LowEnergyNotificationHub *hub = hubMap()->value(qtObject);
    lock.unlock();
    if (!hub)
        return;

    const QBluetoothUuid serviceUuid(QAndroidJniObject(sUuid).toString());
    if (serviceUuid.isNull())
        return;

    const QBluetoothUuid charUuid(QAndroidJniObject(cUuid).toString());
    const QBluetoothUuid descUuid(QAndroidJniObject(dUuid).toString());
    if (charUuid.isNull() || descUuid.isNull())
        return;

    QByteArray payload;
    if (data) { //empty Java byte array is 0x0
        jsize length = env->GetArrayLength(data);
        payload.resize(length);
        env->GetByteArrayRegion(data, 0, length,
                                reinterpret_cast<signed char*>(payload.data()));
    }

    QMetaObject::invokeMethod(hub, "descriptorRead", Qt::QueuedConnection,
                              Q_ARG(QBluetoothUuid, serviceUuid),
                              Q_ARG(QBluetoothUuid, charUuid),
                              Q_ARG(int, handle),
                              Q_ARG(QBluetoothUuid, descUuid),
                              Q_ARG(QByteArray, payload));
}

void LowEnergyNotificationHub::lowEnergy_characteristicWritten(
        JNIEnv *env, jobject, jlong qtObject, jint charHandle,
        jbyteArray data, jint errorCode)
{
    lock.lockForRead();
    LowEnergyNotificationHub *hub = hubMap()->value(qtObject);
    lock.unlock();
    if (!hub)
        return;

    QByteArray payload;
    if (data) { //empty Java byte array is 0x0
        jsize length = env->GetArrayLength(data);
        payload.resize(length);
        env->GetByteArrayRegion(data, 0, length,
                                reinterpret_cast<signed char*>(payload.data()));
    }

    QMetaObject::invokeMethod(hub, "characteristicWritten", Qt::QueuedConnection,
                              Q_ARG(int, charHandle),
                              Q_ARG(QByteArray, payload),
                              Q_ARG(QLowEnergyService::ServiceError,
                                    (QLowEnergyService::ServiceError)errorCode));
}

void LowEnergyNotificationHub::lowEnergy_descriptorWritten(
        JNIEnv *env, jobject, jlong qtObject, jint descHandle,
        jbyteArray data, jint errorCode)
{
    lock.lockForRead();
    LowEnergyNotificationHub *hub = hubMap()->value(qtObject);
    lock.unlock();
    if (!hub)
        return;

    QByteArray payload;
    if (data) { //empty Java byte array is 0x0
        jsize length = env->GetArrayLength(data);
        payload.resize(length);
        env->GetByteArrayRegion(data, 0, length,
                                reinterpret_cast<signed char*>(payload.data()));
    }

    QMetaObject::invokeMethod(hub, "descriptorWritten", Qt::QueuedConnection,
                              Q_ARG(int, descHandle),
                              Q_ARG(QByteArray, payload),
                              Q_ARG(QLowEnergyService::ServiceError,
                                    (QLowEnergyService::ServiceError)errorCode));
}

void LowEnergyNotificationHub::lowEnergy_characteristicChanged(
        JNIEnv *env, jobject, jlong qtObject, jint charHandle, jbyteArray data)
{
    lock.lockForRead();
    LowEnergyNotificationHub *hub = hubMap()->value(qtObject);
    lock.unlock();
    if (!hub)
        return;

    QByteArray payload;
    if (data) { //empty Java byte array is 0x0
        jsize length = env->GetArrayLength(data);
        payload.resize(length);
        env->GetByteArrayRegion(data, 0, length,
                                reinterpret_cast<signed char*>(payload.data()));
    }

    QMetaObject::invokeMethod(hub, "characteristicChanged", Qt::QueuedConnection,
                              Q_ARG(int, charHandle), Q_ARG(QByteArray, payload));
}

void LowEnergyNotificationHub::lowEnergy_serviceError(
        JNIEnv *, jobject, jlong qtObject, jint attributeHandle, int errorCode)
{
    lock.lockForRead();
    LowEnergyNotificationHub *hub = hubMap()->value(qtObject);
    lock.unlock();
    if (!hub)
        return;

    QMetaObject::invokeMethod(hub, "serviceError", Qt::QueuedConnection,
                              Q_ARG(int, attributeHandle),
                              Q_ARG(QLowEnergyService::ServiceError,
                                    (QLowEnergyService::ServiceError)errorCode));
}

QT_END_NAMESPACE
