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

package org.qtproject.qt5.android.bluetooth;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.HashSet;
import java.util.List;

public class QtBluetoothBroadcastReceiver extends BroadcastReceiver
{
    /* Pointer to the Qt object that "owns" the Java object */
    @SuppressWarnings("WeakerAccess")
    long qtObject = 0;
    @SuppressWarnings("WeakerAccess")
    static Context qtContext = null;

    private static final int TURN_BT_ON = 3330;
    private static final int TURN_BT_DISCOVERABLE = 3331;
    private static final String TAG = "QtBluetoothBroadcastReceiver";

    public void onReceive(Context context, Intent intent)
    {
        synchronized (qtContext) {
            if (qtObject == 0)
                return;

            jniOnReceive(qtObject, context, intent);
        }
    }

    public void unregisterReceiver()
    {
        synchronized (qtContext) {
            qtObject = 0;
            qtContext.unregisterReceiver(this);
        }
    }

    public native void jniOnReceive(long qtObject, Context context, Intent intent);

    static public void setContext(Context context)
    {
        qtContext = context;
    }

    static public void setDiscoverable()
    {
        if (!(qtContext instanceof android.app.Activity)) {
            Log.w(TAG, "Discovery mode cannot be enabled from a service.");
            return;
        }

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
        intent.putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION, 300);
        try {
            ((Activity)qtContext).startActivityForResult(intent, TURN_BT_ON);
        } catch (Exception ex) {
            ex.printStackTrace();
        }
    }

    static public void setConnectable()
    {
        if (!(qtContext instanceof android.app.Activity)) {
            Log.w(TAG, "Connectable mode cannot be enabled from a service.");
            return;
        }

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
        try {
            ((Activity)qtContext).startActivityForResult(intent, TURN_BT_DISCOVERABLE);
        } catch (Exception ex) {
            ex.printStackTrace();
        }
    }

    static public boolean setPairingMode(String address, boolean isPairing)
    {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        try {
            BluetoothDevice device = adapter.getRemoteDevice(address);
            String methodName = "createBond";
            if (!isPairing)
                methodName = "removeBond";

            Method m = device.getClass()
                    .getMethod(methodName, (Class[]) null);
            m.invoke(device, (Object[]) null);
        } catch (Exception ex) {
            ex.printStackTrace();
            return false;
        }

        return true;
    }

    /*
     * Returns a list of remote devices confirmed to be connected.
     *
     * This list is not complete as it only detects GATT/BtLE related connections.
     * Unfortunately there is no API that provides the complete list.
     *
     * The function uses Android API v11 & v18. We need to use reflection.
     */
    static public String[] getConnectedDevices()
    {
        try {
            //Bluetooth service name
            Field f = Context.class.getField("BLUETOOTH_SERVICE");
            String serviceValueString = (String)f.get(qtContext);

            Class btProfileClz = Class.forName("android.bluetooth.BluetoothProfile");

            //value of BluetoothProfile.GATT
            f = btProfileClz.getField("GATT");
            int gatt = f.getInt(null);

            //value of BluetoothProfile.GATT_SERVER
            f = btProfileClz.getField("GATT_SERVER");
            int gattServer = f.getInt(null);

            //get BluetoothManager instance
            Object bluetoothManager = qtContext.getSystemService(serviceValueString);

            Class[] cArg = new Class[1];
            cArg[0] = int.class;
            Method m = bluetoothManager.getClass().getMethod("getConnectedDevices", cArg);

            List gattConnections = (List) m.invoke(bluetoothManager, gatt);
            List gattServerConnections = (List) m.invoke(bluetoothManager, gattServer);

            //process found remote connections but avoid duplications
            HashSet<String> set = new HashSet<String>();
            for (Object gattConnection : gattConnections)
                set.add(gattConnection.toString());

            for (Object gattServerConnection : gattServerConnections)
                set.add(gattServerConnection.toString());

            return set.toArray(new String[set.size()]);
        } catch (Exception ex) {
            //API is less than 18
            return new String[0];
        }
    }
}
