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

#include "android/servicediscoverybroadcastreceiver_p.h"
#include <QtCore/QLoggingCategory>
#include <QtCore/private/qjnihelpers_p.h>
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <QtBluetooth/QBluetoothAddress>
#include <QtBluetooth/QBluetoothDeviceInfo>
#include "android/jni_android_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

ServiceDiscoveryBroadcastReceiver::ServiceDiscoveryBroadcastReceiver(QObject* parent): AndroidBroadcastReceiver(parent)
{
    if (QtAndroidPrivate::androidSdkVersion() >= 15)
        addAction(valueForStaticField(JavaNames::BluetoothDevice, JavaNames::ActionUuid)); //API 15+
}

void ServiceDiscoveryBroadcastReceiver::onReceive(JNIEnv *env, jobject context, jobject intent)
{
    Q_UNUSED(context);
    Q_UNUSED(env);

    QAndroidJniObject intentObject(intent);
    const QString action = intentObject.callObjectMethod("getAction", "()Ljava/lang/String;").toString();

    qCDebug(QT_BT_ANDROID) << "ServiceDiscoveryBroadcastReceiver::onReceive() - event:" << action;

    if (action == valueForStaticField(JavaNames::BluetoothDevice,
                                      JavaNames::ActionUuid).toString()) {

        QAndroidJniObject keyExtra = valueForStaticField(JavaNames::BluetoothDevice,
                                                         JavaNames::ExtraUuid);
        QAndroidJniObject parcelableUuids = intentObject.callObjectMethod(
                                                "getParcelableArrayExtra",
                                                "(Ljava/lang/String;)[Landroid/os/Parcelable;",
                                                keyExtra.object<jstring>());
        if (!parcelableUuids.isValid()) {
            emit uuidFetchFinished(QBluetoothAddress(), QList<QBluetoothUuid>());
            return;
        }
        const QList<QBluetoothUuid> result = ServiceDiscoveryBroadcastReceiver::convertParcelableArray(parcelableUuids);

        keyExtra = valueForStaticField(JavaNames::BluetoothDevice, JavaNames::ExtraDevice);
        QAndroidJniObject bluetoothDevice =
        intentObject.callObjectMethod("getParcelableExtra",
                                  "(Ljava/lang/String;)Landroid/os/Parcelable;",
                                  keyExtra.object<jstring>());
        QBluetoothAddress address;
        if (bluetoothDevice.isValid()) {
            address = QBluetoothAddress(bluetoothDevice.callObjectMethod<jstring>("getAddress").toString());
            emit uuidFetchFinished(address, result);
        } else {
            emit uuidFetchFinished(QBluetoothAddress(), QList<QBluetoothUuid>());
        }
    }
}

QList<QBluetoothUuid> ServiceDiscoveryBroadcastReceiver::convertParcelableArray(const QAndroidJniObject &parcelUuidArray)
{
    QList<QBluetoothUuid> result;
    QAndroidJniEnvironment env;
    QAndroidJniObject p;

    jobjectArray parcels = parcelUuidArray.object<jobjectArray>();
    if (!parcels)
        return result;

    jint size = env->GetArrayLength(parcels);
    for (int i = 0; i < size; i++) {
        p = env->GetObjectArrayElement(parcels, i);

        QBluetoothUuid uuid(p.callObjectMethod<jstring>("toString").toString());
        //qCDebug(QT_BT_ANDROID) << uuid.toString();
        result.append(uuid);
    }

    return result;
}

QT_END_NAMESPACE

