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

package org.qtproject.qt5.android.bluetooth;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.content.Context;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseData.Builder;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.os.ParcelUuid;
import android.util.Log;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;
import java.util.HashMap;
import java.util.UUID;

public class QtBluetoothLEServer {
    private static final String TAG = "QtBluetoothGattServer";

    /* Pointer to the Qt object that "owns" the Java object */
    @SuppressWarnings({"CanBeFinal", "WeakerAccess"})
    long qtObject = 0;
    @SuppressWarnings("WeakerAccess")

    private Context qtContext = null;

    // Bluetooth members
    private final BluetoothAdapter mBluetoothAdapter;
    private BluetoothGattServer mGattServer = null;
    private BluetoothLeAdvertiser mLeAdvertiser = null;

    /*
        As per Bluetooth specification each connected device can have individual and persistent
        Client characteristic configurations (see Bluetooth Spec 5.0 Vol 3 Part G 3.3.3.3)
        This class manages the existing configurrations.
     */
    private class ClientCharacteristicManager {
        private final HashMap<BluetoothGattCharacteristic, List<Entry>> notificationStore = new HashMap<BluetoothGattCharacteristic, List<Entry>>();

        private class Entry {
            BluetoothDevice device = null;
            byte[] value = null;
            boolean isConnected = false;
        }

        public void insertOrUpdate(BluetoothGattCharacteristic characteristic,
                              BluetoothDevice device, byte[] newValue)
        {
            if (notificationStore.containsKey(characteristic)) {

                List<Entry> entries = notificationStore.get(characteristic);
                for (int i = 0; i < entries.size(); i++) {
                    if (entries.get(i).device.equals(device)) {
                        Entry e = entries.get(i);
                        e.value = newValue;
                        entries.set(i, e);
                        return;
                    }
                }

                // not match so far -> add device to list
                Entry e = new Entry();
                e.device = device;
                e.value = newValue;
                e.isConnected = true;
                entries.add(e);
                return;
            }

            // new characteristic
            Entry e = new Entry();
            e.device = device;
            e.value = newValue;
            e.isConnected = true;
            List<Entry> list = new LinkedList<Entry>();
            list.add(e);
            notificationStore.put(characteristic, list);
        }

        /*
            Marks client characteristic configuration entries as (in)active based the associated
            devices general connectivity state.
            This function avoids that existing configurations are not acted
            upon when the associated device is not connected.
         */
        public void markDeviceConnectivity(BluetoothDevice device, boolean isConnected)
        {
            final Iterator<BluetoothGattCharacteristic> keys = notificationStore.keySet().iterator();
            while (keys.hasNext()) {
                final BluetoothGattCharacteristic characteristic = keys.next();
                final List<Entry> entries = notificationStore.get(characteristic);
                if (entries == null)
                    continue;

                ListIterator<Entry> charConfig = entries.listIterator();
                while (charConfig.hasNext()) {
                    Entry e = charConfig.next();
                    if (e.device.equals(device))
                        e.isConnected = isConnected;
                }
            }
        }

        // Returns list of all BluetoothDevices which require notification or indication.
        // No match returns an empty list.
        List<BluetoothDevice> getToBeUpdatedDevices(BluetoothGattCharacteristic characteristic)
        {
            ArrayList<BluetoothDevice> result = new ArrayList<BluetoothDevice>();
            if (!notificationStore.containsKey(characteristic))
                return result;

            final ListIterator<Entry> iter = notificationStore.get(characteristic).listIterator();
            while (iter.hasNext())
                result.add(iter.next().device);

            return result;
        }

        // Returns null if no match; otherwise the configured actual client characteristic
        // configuration value
        byte[] valueFor(BluetoothGattCharacteristic characteristic, BluetoothDevice device)
        {
            if (!notificationStore.containsKey(characteristic))
                return null;

            List<Entry> entries = notificationStore.get(characteristic);
            for (int i = 0; i < entries.size(); i++) {
                final Entry entry = entries.get(i);
                if (entry.device.equals(device) && entry.isConnected == true)
                    return entries.get(i).value;
            }

            return null;
        }
    }

    private static final UUID CLIENT_CHARACTERISTIC_CONFIGURATION_UUID = UUID
            .fromString("00002902-0000-1000-8000-00805f9b34fb");
    ClientCharacteristicManager clientCharacteristicManager = new ClientCharacteristicManager();

    public QtBluetoothLEServer(Context context)
    {
        qtContext = context;
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();

        if (mBluetoothAdapter == null || qtContext == null) {
            Log.w(TAG, "Missing Bluetooth adapter or Qt context. Peripheral role disabled.");
            return;
        }

        BluetoothManager manager = (BluetoothManager) qtContext.getSystemService(Context.BLUETOOTH_SERVICE);
        if (manager == null) {
            Log.w(TAG, "Bluetooth service not available.");
            return;
        }

        mGattServer = manager.openGattServer(qtContext, mGattServerListener);
        mLeAdvertiser = mBluetoothAdapter.getBluetoothLeAdvertiser();

        if (!mBluetoothAdapter.isMultipleAdvertisementSupported())
            Log.w(TAG, "Device does not support Bluetooth Low Energy advertisement.");
        else
            Log.w(TAG, "Let's do BTLE Peripheral.");
    }

    /*
     * Call back handler for the Gatt Server.
     */
    private BluetoothGattServerCallback mGattServerListener = new BluetoothGattServerCallback()
    {
        @Override
        public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
            Log.w(TAG, "Our gatt server connection state changed, new state: " + newState);
            super.onConnectionStateChange(device, status, newState);

            int qtControllerState = 0;
            switch (newState) {
                case BluetoothProfile.STATE_DISCONNECTED:
                    qtControllerState = 0; // QLowEnergyController::UnconnectedState
                    clientCharacteristicManager.markDeviceConnectivity(device, false);
                    break;
                case BluetoothProfile.STATE_CONNECTED:
                    clientCharacteristicManager.markDeviceConnectivity(device, true);
                    qtControllerState = 2; // QLowEnergyController::ConnectedState
                    break;
            }

            int qtErrorCode;
            switch (status) {
                case BluetoothGatt.GATT_SUCCESS:
                    qtErrorCode = 0; break;
                default:
                    Log.w(TAG, "Unhandled error code on peripheral connectionStateChanged: " + status + " " + newState);
                    qtErrorCode = status;
                    break;
            }

            leServerConnectionStateChange(qtObject, qtErrorCode, qtControllerState);
        }

        @Override
        public void onServiceAdded(int status, BluetoothGattService service) {
            super.onServiceAdded(status, service);
        }

        @Override
        public void onCharacteristicReadRequest(BluetoothDevice device, int requestId, int offset, BluetoothGattCharacteristic characteristic)
        {
            byte[] dataArray;
            try {
                dataArray = Arrays.copyOfRange(characteristic.getValue(), offset, characteristic.getValue().length);
                mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, dataArray);
            } catch (Exception ex) {
                Log.w(TAG, "onCharacteristicReadRequest: " + requestId + " " + offset + " " + characteristic.getValue().length);
                ex.printStackTrace();
                mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_FAILURE, offset, null);
            }

            super.onCharacteristicReadRequest(device, requestId, offset, characteristic);
        }

        @Override
        public void onCharacteristicWriteRequest(BluetoothDevice device, int requestId, BluetoothGattCharacteristic characteristic,
                                                 boolean preparedWrite, boolean responseNeeded, int offset, byte[] value)
        {
            Log.w(TAG, "onCharacteristicWriteRequest");
            int resultStatus = BluetoothGatt.GATT_SUCCESS;
            boolean sendNotificationOrIndication = false;
            if (!preparedWrite) { // regular write
                if (offset == 0) {
                    characteristic.setValue(value);
                    leServerCharacteristicChanged(qtObject, characteristic, value);
                    sendNotificationOrIndication = true;
                } else {
                    // This should not really happen as per Bluetooth spec
                    Log.w(TAG, "onCharacteristicWriteRequest: !preparedWrite, offset " + offset + ", Not supported");
                    resultStatus = BluetoothGatt.GATT_REQUEST_NOT_SUPPORTED;
                }


            } else {
                Log.w(TAG, "onCharacteristicWriteRequest: preparedWrite, offset " + offset + ", Not supported");
                resultStatus = BluetoothGatt.GATT_REQUEST_NOT_SUPPORTED;

                // TODO we need to record all requests and execute them in one go once onExecuteWrite() is received
                // we use a queue to remember the pending requests
                // TODO we are ignoring the device identificator for now -> Bluetooth spec requires a queue per device
            }


            if (responseNeeded)
                mGattServer.sendResponse(device, requestId, resultStatus, offset, value);
            if (sendNotificationOrIndication)
                sendNotificationsOrIndications(characteristic);

            super.onCharacteristicWriteRequest(device, requestId, characteristic, preparedWrite, responseNeeded, offset, value);
        }

        @Override
        public void onDescriptorReadRequest(BluetoothDevice device, int requestId, int offset, BluetoothGattDescriptor descriptor)
        {
            byte[] dataArray = descriptor.getValue();
            try {
                if (descriptor.getUuid().equals(CLIENT_CHARACTERISTIC_CONFIGURATION_UUID)) {
                    dataArray = clientCharacteristicManager.valueFor(descriptor.getCharacteristic(), device);
                    if (dataArray == null)
                        dataArray = descriptor.getValue();
                }

                dataArray = Arrays.copyOfRange(dataArray, offset, dataArray.length);
                mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, dataArray);
            } catch (Exception ex) {
                Log.w(TAG, "onDescriptorReadRequest: " + requestId + " " + offset + " " + dataArray.length);
                ex.printStackTrace();
                mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_FAILURE, offset, null);
            }

            super.onDescriptorReadRequest(device, requestId, offset, descriptor);
        }

        @Override
        public void onDescriptorWriteRequest(BluetoothDevice device, int requestId, BluetoothGattDescriptor descriptor,
                                             boolean preparedWrite, boolean responseNeeded, int offset, byte[] value)
        {
            int resultStatus = BluetoothGatt.GATT_SUCCESS;
            if (!preparedWrite) { // regular write
                if (offset == 0) {
                    descriptor.setValue(value);

                    if (descriptor.getUuid().equals(CLIENT_CHARACTERISTIC_CONFIGURATION_UUID)) {
                        clientCharacteristicManager.insertOrUpdate(descriptor.getCharacteristic(),
                                                                   device, value);
                    }

                    leServerDescriptorWritten(qtObject, descriptor, value);
                } else {
                    // This should not really happen as per Bluetooth spec
                    Log.w(TAG, "onDescriptorWriteRequest: !preparedWrite, offset " + offset + ", Not supported");
                    resultStatus = BluetoothGatt.GATT_REQUEST_NOT_SUPPORTED;
                }


            } else {
                Log.w(TAG, "onDescriptorWriteRequest: preparedWrite, offset " + offset + ", Not supported");
                resultStatus = BluetoothGatt.GATT_REQUEST_NOT_SUPPORTED;
                // TODO we need to record all requests and execute them in one go once onExecuteWrite() is received
                // we use a queue to remember the pending requests
                // TODO we are ignoring the device identificator for now -> Bluetooth spec requires a queue per device
            }


            if (responseNeeded)
                mGattServer.sendResponse(device, requestId, resultStatus, offset, value);

            super.onDescriptorWriteRequest(device, requestId, descriptor, preparedWrite, responseNeeded, offset, value);
        }

        @Override
        public void onExecuteWrite(BluetoothDevice device, int requestId, boolean execute)
        {
            // TODO not yet implemented -> return proper GATT error for it
            mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_REQUEST_NOT_SUPPORTED, 0, null);

            super.onExecuteWrite(device, requestId, execute);
        }

        @Override
        public void onNotificationSent(BluetoothDevice device, int status) {
            super.onNotificationSent(device, status);
            Log.w(TAG, "onNotificationSent" + device + " " + status);
        }

        // MTU change disabled since it requires API level 22. Right now we only enforce lvl 21
//        @Override
//        public void onMtuChanged(BluetoothDevice device, int mtu) {
//            super.onMtuChanged(device, mtu);
//        }
    };

    public boolean connectServer()
    {
        if (mGattServer == null)
            return false;

        return true;
    }

    public void disconnectServer()
    {
        if (mGattServer == null)
            return;

        mGattServer.close();
    }

    public boolean startAdvertising(AdvertiseData advertiseData,
                                    AdvertiseData scanResponse,
                                    AdvertiseSettings settings)
    {
        if (mLeAdvertiser == null)
            return false;

        connectServer();

        Log.w(TAG, "Starting to advertise.");
        mLeAdvertiser.startAdvertising(settings, advertiseData, scanResponse, mAdvertiseListener);

        return true;
    }

    public void stopAdvertising()
    {
        if (mLeAdvertiser == null)
            return;

        mLeAdvertiser.stopAdvertising(mAdvertiseListener);
        Log.w(TAG, "Advertisement stopped.");
    }

    public void addService(BluetoothGattService service)
    {
        if (mGattServer == null)
            return;

        mGattServer.addService(service);
    }

    /*
        Check the client characteristics configuration for the given characteristic
        and sends notifications or indications as per required.
     */
    private void sendNotificationsOrIndications(BluetoothGattCharacteristic characteristic)
    {
        final ListIterator<BluetoothDevice> iter =
                clientCharacteristicManager.getToBeUpdatedDevices(characteristic).listIterator();

        // TODO This quick loop over multiple devices should be synced with onNotificationSent().
        //      The next notifyCharacteristicChanged() call must wait until onNotificationSent()
        //      was received. At this becomes an issue when the server accepts multiple remote
        //      devices at the same time.
        while (iter.hasNext()) {
            final BluetoothDevice device = iter.next();
            final byte[] clientCharacteristicConfig = clientCharacteristicManager.valueFor(characteristic, device);
            if (clientCharacteristicConfig != null) {
                if (Arrays.equals(clientCharacteristicConfig, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)) {
                    mGattServer.notifyCharacteristicChanged(device, characteristic, false);
                } else if (Arrays.equals(clientCharacteristicConfig, BluetoothGattDescriptor.ENABLE_INDICATION_VALUE)) {
                    mGattServer.notifyCharacteristicChanged(device, characteristic, true);
                }
            }
        }
    }

    /*
        Updates the local database value for the given characteristic with \a charUuid and
        \a newValue. If notifications for this task are enabled an approproiate notification will
        be send to the remote client.

        This function is called from the Qt thread.
     */
    public boolean writeCharacteristic(BluetoothGattService service, UUID charUuid, byte[] newValue)
    {
        BluetoothGattCharacteristic foundChar = null;
        List<BluetoothGattCharacteristic> charList = service.getCharacteristics();
        for (BluetoothGattCharacteristic iter: charList) {
            if (iter.getUuid().equals(charUuid) && foundChar == null) {
                foundChar = iter;
                // don't break here since we want to check next condition below on next iteration
            } else if (iter.getUuid().equals(charUuid)) {
                Log.w(TAG, "Found second char with same UUID. Wrong char may have been selected.");
                break;
            }
        }

        if (foundChar == null) {
            Log.w(TAG, "writeCharacteristic: update for unknown characteristic failed");
            return false;
        }

        foundChar.setValue(newValue);
        sendNotificationsOrIndications(foundChar);

        return true;
    }

    /*
        Updates the local database value for the given \a descUuid to \a newValue.

        This function is called from the Qt thread.
     */
    public boolean writeDescriptor(BluetoothGattService service, UUID charUuid, UUID descUuid,
                                   byte[] newValue)
    {
        BluetoothGattDescriptor foundDesc = null;
        BluetoothGattCharacteristic foundChar = null;
        final List<BluetoothGattCharacteristic> charList = service.getCharacteristics();
        for (BluetoothGattCharacteristic iter: charList) {
            if (!iter.getUuid().equals(charUuid))
                continue;

            if (foundChar == null) {
                foundChar = iter;
            } else {
                Log.w(TAG, "Found second char with same UUID. Wrong char may have been selected.");
                break;
            }
        }

        if (foundChar != null)
            foundDesc = foundChar.getDescriptor(descUuid);

        if (foundChar == null || foundDesc == null) {
            Log.w(TAG, "writeDescriptor: update for unknown char or desc failed (" + foundChar + ")");
            return false;
        }

        // we even write CLIENT_CHARACTERISTIC_CONFIGURATION_UUID this way as we choose
        // to interpret the server's call as a change of the default value.
        foundDesc.setValue(newValue);

        return true;
    }

    /*
     * Call back handler for Advertisement requests.
     */
    private AdvertiseCallback mAdvertiseListener = new AdvertiseCallback()
    {
        @Override
        public void onStartSuccess(AdvertiseSettings settingsInEffect) {
            super.onStartSuccess(settingsInEffect);
        }

        @Override
        public void onStartFailure(int errorCode) {
            Log.e(TAG, "Advertising failure: " + errorCode);
            super.onStartFailure(errorCode);

            // changing errorCode here implies changes to errorCode handling on Qt side
            int qtErrorCode = 0;
            switch (errorCode) {
                case AdvertiseCallback.ADVERTISE_FAILED_ALREADY_STARTED:
                    return; // ignore -> noop
                case AdvertiseCallback.ADVERTISE_FAILED_DATA_TOO_LARGE:
                    qtErrorCode = 1;
                    break;
                case AdvertiseCallback.ADVERTISE_FAILED_FEATURE_UNSUPPORTED:
                    qtErrorCode = 2;
                    break;
                default: // default maps to internal error
                case AdvertiseCallback.ADVERTISE_FAILED_INTERNAL_ERROR:
                    qtErrorCode = 3;
                    break;
                case AdvertiseCallback.ADVERTISE_FAILED_TOO_MANY_ADVERTISERS:
                    qtErrorCode = 4;
                    break;
            }

            if (qtErrorCode > 0)
                leServerAdvertisementError(qtObject, qtErrorCode);
        }
    };

    public native void leServerConnectionStateChange(long qtObject, int errorCode, int newState);
    public native void leServerAdvertisementError(long qtObject, int status);
    public native void leServerCharacteristicChanged(long qtObject,
                                                     BluetoothGattCharacteristic characteristic,
                                                     byte[] newValue);
    public native void leServerDescriptorWritten(long qtObject,
                                                 BluetoothGattDescriptor descriptor,
                                                 byte[] newValue);
}
