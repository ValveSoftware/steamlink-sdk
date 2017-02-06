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

#include <QtCore/QLoggingCategory>
#include <QtCore/private/qjnihelpers_p.h>
#include "localdevicebroadcastreceiver_p.h"
#include "android/jni_android_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

const char *scanModes[] = {"SCAN_MODE_NONE", "SCAN_MODE_CONNECTABLE", "SCAN_MODE_CONNECTABLE_DISCOVERABLE"};
const char *bondModes[] = {"BOND_NONE", "BOND_BONDING", "BOND_BONDED"};

LocalDeviceBroadcastReceiver::LocalDeviceBroadcastReceiver(QObject *parent) :
    AndroidBroadcastReceiver(parent), previousScanMode(0)
{
    addAction(valueForStaticField(JavaNames::BluetoothDevice, JavaNames::ActionBondStateChanged));
    addAction(valueForStaticField(JavaNames::BluetoothAdapter, JavaNames::ActionScanModeChanged));
    addAction(valueForStaticField(JavaNames::BluetoothDevice, JavaNames::ActionAclConnected));
    addAction(valueForStaticField(JavaNames::BluetoothDevice, JavaNames::ActionAclDisconnected));
    if (QtAndroidPrivate::androidSdkVersion() >= 15)
        addAction(valueForStaticField(JavaNames::BluetoothDevice, JavaNames::ActionPairingRequest)); //API 15

    //cache integer values for host & bonding mode
    //don't use the java fields directly but refer to them by name
    QAndroidJniEnvironment env;
    for (uint i = 0; i < (sizeof(hostModePreset)/sizeof(hostModePreset[0])); i++) {
        hostModePreset[i] = QAndroidJniObject::getStaticField<jint>(
                                                "android/bluetooth/BluetoothAdapter",
                                                scanModes[i]);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            hostModePreset[i] = 0;
        }
    }

    for (uint i = 0; i < (sizeof(bondingModePreset)/sizeof(bondingModePreset[0])); i++) {
        bondingModePreset[i] = QAndroidJniObject::getStaticField<jint>(
                                                "android/bluetooth/BluetoothDevice",
                                                bondModes[i]);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            bondingModePreset[i] = 0;
        }
    }
}

void LocalDeviceBroadcastReceiver::onReceive(JNIEnv *env, jobject context, jobject intent)
{
    Q_UNUSED(context);
    Q_UNUSED(env);

    QAndroidJniObject intentObject(intent);
    const QString action = intentObject.callObjectMethod("getAction", "()Ljava/lang/String;").toString();
    qCDebug(QT_BT_ANDROID) << QStringLiteral("LocalDeviceBroadcastReceiver::onReceive() - event: %1").arg(action);

    if (action == valueForStaticField(JavaNames::BluetoothAdapter,
                                      JavaNames::ActionScanModeChanged).toString()) {

        const QAndroidJniObject extrasBundle =
                intentObject.callObjectMethod("getExtras","()Landroid/os/Bundle;");
        const QAndroidJniObject keyExtra = valueForStaticField(JavaNames::BluetoothAdapter,
                                                               JavaNames::ExtraScanMode);

        int extra = extrasBundle.callMethod<jint>("getInt",
                                                  "(Ljava/lang/String;)I",
                                                  keyExtra.object<jstring>());

        if (previousScanMode != extra) {
            previousScanMode = extra;

            if (extra == hostModePreset[0])
                emit hostModeStateChanged(QBluetoothLocalDevice::HostPoweredOff);
            else if (extra == hostModePreset[1])
                emit hostModeStateChanged(QBluetoothLocalDevice::HostConnectable);
            else if (extra == hostModePreset[2])
                emit hostModeStateChanged(QBluetoothLocalDevice::HostDiscoverable);
            else
                qCWarning(QT_BT_ANDROID) << "Unknown Host State";
        }
    } else if (action == valueForStaticField(JavaNames::BluetoothDevice,
                                             JavaNames::ActionBondStateChanged).toString()) {
        //get BluetoothDevice
        QAndroidJniObject keyExtra = valueForStaticField(JavaNames::BluetoothDevice,
                                                         JavaNames::ExtraDevice);
        const QAndroidJniObject bluetoothDevice =
                intentObject.callObjectMethod("getParcelableExtra",
                                              "(Ljava/lang/String;)Landroid/os/Parcelable;",
                                              keyExtra.object<jstring>());

        //get new bond state
        keyExtra = valueForStaticField(JavaNames::BluetoothDevice, JavaNames::ExtraBondState);
        const QAndroidJniObject extrasBundle =
                intentObject.callObjectMethod("getExtras","()Landroid/os/Bundle;");
        int bondState = extrasBundle.callMethod<jint>("getInt",
                                                      "(Ljava/lang/String;)I",
                                                      keyExtra.object<jstring>());

        QBluetoothAddress address(bluetoothDevice.callObjectMethod<jstring>("getAddress").toString());
        if (address.isNull())
                return;

        if (bondState == bondingModePreset[0])
            emit pairingStateChanged(address, QBluetoothLocalDevice::Unpaired);
        else if (bondState == bondingModePreset[1])
            ; //we ignore this as Qt doesn't have equivalent API value
        else if (bondState == bondingModePreset[2])
            emit pairingStateChanged(address, QBluetoothLocalDevice::Paired);
        else
            qCWarning(QT_BT_ANDROID) << "Unknown BOND_STATE_CHANGED value:" << bondState;

    } else if (action == valueForStaticField(JavaNames::BluetoothDevice,
                                             JavaNames::ActionAclConnected).toString() ||
               action == valueForStaticField(JavaNames::BluetoothDevice,
                                             JavaNames::ActionAclDisconnected).toString()) {

        const QString connectEvent = valueForStaticField(JavaNames::BluetoothDevice,
                                                         JavaNames::ActionAclConnected).toString();
        const bool isConnectEvent =
                action == connectEvent ? true : false;

        //get BluetoothDevice
        const QAndroidJniObject keyExtra = valueForStaticField(JavaNames::BluetoothDevice,
                                                               JavaNames::ExtraDevice);
        QAndroidJniObject bluetoothDevice =
                intentObject.callObjectMethod("getParcelableExtra",
                                              "(Ljava/lang/String;)Landroid/os/Parcelable;",
                                              keyExtra.object<jstring>());

        QBluetoothAddress address(bluetoothDevice.callObjectMethod<jstring>("getAddress").toString());
        if (address.isNull())
            return;

        emit connectDeviceChanges(address, isConnectEvent);
    } else if (action == valueForStaticField(JavaNames::BluetoothDevice,
                                             JavaNames::ActionPairingRequest).toString()) {

        QAndroidJniObject keyExtra = valueForStaticField(JavaNames::BluetoothDevice,
                                                         JavaNames::ExtraPairingVariant);
        int variant = intentObject.callMethod<jint>("getIntExtra",
                                                    "(Ljava/lang/String;I)I",
                                                    keyExtra.object<jstring>(),
                                                    -1);

        int key = -1;
        switch (variant) {
        case -1: //ignore -> no pairing variant set
            return;
        case 2: //BluetoothDevice.PAIRING_VARIANT_PASSKEY_CONFIRMATION
        {
            keyExtra = valueForStaticField(JavaNames::BluetoothDevice,
                                           JavaNames::ExtraPairingKey);
            key = intentObject.callMethod<jint>("getIntExtra",
                                                    "(Ljava/lang/String;I)I",
                                                    keyExtra.object<jstring>(),
                                                    -1);
            if (key == -1)
                return;

            keyExtra = valueForStaticField(JavaNames::BluetoothDevice, JavaNames::ExtraDevice);
            QAndroidJniObject bluetoothDevice =
                    intentObject.callObjectMethod("getParcelableExtra",
                                                  "(Ljava/lang/String;)Landroid/os/Parcelable;",
                                                  keyExtra.object<jstring>());

            //we need to keep a reference around in case the user confirms later on
            pairingDevice = bluetoothDevice;

            QBluetoothAddress address(bluetoothDevice.callObjectMethod<jstring>("getAddress").toString());

            //User has choice to confirm or not. If no confirmation is happening
            //the OS default pairing dialog can be used or timeout occurs.
            emit pairingDisplayConfirmation(address, QString::number(key));
            break;
        }
        default:
            qCWarning(QT_BT_ANDROID) << "Unknown pairing variant: " << variant;
            return;
        }
    }
}

bool LocalDeviceBroadcastReceiver::pairingConfirmation(bool accept)
{
    if (!pairingDevice.isValid())
        return false;

    bool success = pairingDevice.callMethod<jboolean>("setPairingConfirmation",
                                              "(Z)Z", accept);
    pairingDevice = QAndroidJniObject();
    return success;
}

QT_END_NAMESPACE
