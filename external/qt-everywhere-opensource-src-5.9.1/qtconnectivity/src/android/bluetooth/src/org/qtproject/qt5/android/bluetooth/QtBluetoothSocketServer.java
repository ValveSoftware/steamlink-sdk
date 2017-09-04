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

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.util.Log;
import java.io.IOException;
import java.util.UUID;

@SuppressWarnings("WeakerAccess")
public class QtBluetoothSocketServer extends Thread
{

    /* Pointer to the Qt object that "owns" the Java object */
    @SuppressWarnings({"WeakerAccess", "CanBeFinal"})
    long qtObject = 0;
    @SuppressWarnings({"WeakerAccess", "CanBeFinal"})
    public boolean logEnabled = false;

    private static final String TAG = "QtBluetooth";
    private boolean m_isSecure = false;
    private UUID m_uuid;
    private String m_serviceName;
    private BluetoothServerSocket m_serverSocket = null;

    //error codes
    private static final int QT_NO_BLUETOOTH_SUPPORTED = 0;
    private static final int QT_LISTEN_FAILED = 1;
    private static final int QT_ACCEPT_FAILED = 2;

    public QtBluetoothSocketServer()
    {
        setName("QtSocketServerThread");
    }

    public void setServiceDetails(String uuid, String serviceName, boolean isSecure)
    {
        m_uuid = UUID.fromString(uuid);
        m_serviceName = serviceName;
        m_isSecure = isSecure;

    }

    public void run()
    {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter == null) {
            errorOccurred(qtObject, QT_NO_BLUETOOTH_SUPPORTED);
            return;
        }

        try {
            if (m_isSecure) {
                m_serverSocket = adapter.listenUsingRfcommWithServiceRecord(m_serviceName, m_uuid);
                if (logEnabled)
                    Log.d(TAG, "Using secure socket listener");
            } else {
                m_serverSocket = adapter.listenUsingInsecureRfcommWithServiceRecord(m_serviceName, m_uuid);
                if (logEnabled)
                    Log.d(TAG, "Using insecure socket listener");
            }
        } catch (IOException ex) {
            if (logEnabled)
                Log.d(TAG, "Server socket listen() failed:" + ex.toString());
            ex.printStackTrace();
            errorOccurred(qtObject, QT_LISTEN_FAILED);
            return;
        }

        BluetoothSocket s;
        if (m_serverSocket != null) {
            try {
                while (!isInterrupted()) {
                    //this blocks until we see incoming connection
                    //or close() is called
                    if (logEnabled)
                        Log.d(TAG, "Waiting for new incoming socket");
                    s = m_serverSocket.accept();

                    if (logEnabled)
                        Log.d(TAG, "New socket accepted");
                    newSocket(qtObject, s);
                }
            } catch (IOException ex) {
                if (logEnabled)
                    Log.d(TAG, "Server socket accept() failed:" + ex.toString());
                ex.printStackTrace();
                errorOccurred(qtObject, QT_ACCEPT_FAILED);
            }
        }

        Log.d(TAG, "Leaving server socket thread.");
    }

    public void close()
    {
        if (!isAlive())
            return;

        try {
            //ensure closing of thread if we are not currently blocking on accept()
            interrupt();

            //interrupts accept() call above
            if (m_serverSocket != null)
                m_serverSocket.close();
        } catch (IOException ex) {
            Log.d(TAG, "Closing server socket close() failed:" + ex.toString());
            ex.printStackTrace();
        }
    }

    public static native void errorOccurred(long qtObject, int errorCode);
    public static native void newSocket(long qtObject, BluetoothSocket socket);
}
