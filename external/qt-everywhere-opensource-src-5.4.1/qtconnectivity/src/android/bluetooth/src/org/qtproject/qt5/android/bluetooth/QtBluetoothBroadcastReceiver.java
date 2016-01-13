/****************************************************************************
**
** Copyright (C) 2013 Lauri Laanmets (Proekspert AS) <lauri.laanmets@eesti.ee>
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

package org.qtproject.qt5.android.bluetooth;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.HashSet;
import java.util.List;

public class QtBluetoothBroadcastReceiver extends BroadcastReceiver
{
    /* Pointer to the Qt object that "owns" the Java object */
    long qtObject = 0;
    static Activity qtactivity = null;

    private static final int TURN_BT_ON = 3330;
    private static final int TURN_BT_DISCOVERABLE = 3331;

    public void onReceive(Context context, Intent intent)
    {
        synchronized (qtactivity) {
            if (qtObject == 0)
                return;

            jniOnReceive(qtObject, context, intent);
        }
    }

    public void unregisterReceiver()
    {
        synchronized (qtactivity) {
            qtObject = 0;
            qtactivity.unregisterReceiver(this);
        }
    }

    public native void jniOnReceive(long qtObject, Context context, Intent intent);

    static public void setActivity(Activity activity, Object activityDelegate)
    {
        qtactivity = activity;
    }

    static public void setDiscoverable()
    {
        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
        intent.putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION, 300);
        try {
            qtactivity.startActivityForResult(intent, TURN_BT_ON);
        } catch (Exception ex) {
            ex.printStackTrace();
        }
    }

    static public void setConnectable()
    {
        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
        try {
            qtactivity.startActivityForResult(intent, TURN_BT_DISCOVERABLE);
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
            String serviceValueString = (String)f.get(qtactivity);

            Class btProfileClz = Class.forName("android.bluetooth.BluetoothProfile");

            //value of BluetoothProfile.GATT
            f = btProfileClz.getField("GATT");
            int gatt = f.getInt(null);

            //value of BluetoothProfile.GATT_SERVER
            f = btProfileClz.getField("GATT_SERVER");
            int gattServer = f.getInt(null);

            //get BluetoothManager instance
            Object bluetoothManager = qtactivity.getSystemService(serviceValueString);

            Class[] cArg = new Class[1];
            cArg[0] = int.class;
            Method m = bluetoothManager.getClass().getMethod("getConnectedDevices", cArg);

            List gattConnections = (List) m.invoke(bluetoothManager, gatt);
            List gattServerConnections = (List) m.invoke(bluetoothManager, gattServer);

            //process found remote connections but avoid duplications
            HashSet<String> set = new HashSet<String>();
            for (int i = 0; i < gattConnections.size(); i++)
                set.add(gattConnections.get(i).toString());

            for (int i = 0; i < gattServerConnections.size(); i++)
                set.add(gattServerConnections.get(i).toString());

            return set.toArray(new String[set.size()]);
        } catch (Exception ex) {
            //API is less than 18
            return new String[0];
        }
    }
}
