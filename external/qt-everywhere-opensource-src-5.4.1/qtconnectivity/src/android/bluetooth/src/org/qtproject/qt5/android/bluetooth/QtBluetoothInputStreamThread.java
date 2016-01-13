/****************************************************************************
**
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

import java.io.InputStream;
import java.io.IOException;
import android.util.Log;

public class QtBluetoothInputStreamThread extends Thread
{
    /* Pointer to the Qt object that "owns" the Java object */
    long qtObject = 0;
    public boolean logEnabled = false;
    private static final String TAG = "QtBluetooth";
    private InputStream m_inputStream = null;

    //error codes
    public static final int QT_MISSING_INPUT_STREAM = 0;
    public static final int QT_READ_FAILED = 1;
    public static final int QT_THREAD_INTERRUPTED = 2;

    public QtBluetoothInputStreamThread()
    {
        setName("QtBtInputStreamThread");
    }

    public void setInputStream(InputStream stream)
    {
        m_inputStream = stream;
    }

    public void run()
    {
        if (m_inputStream == null) {
            errorOccurred(qtObject, QT_MISSING_INPUT_STREAM);
            return;
        }

        byte[] buffer = new byte[1000];
        int bytesRead = 0;

        try {
            while (!isInterrupted()) {
                //this blocks until we see incoming data
                //or close() on related BluetoothSocket is called
                bytesRead = m_inputStream.read(buffer);
                readyData(qtObject, buffer, bytesRead);
            }

            errorOccurred(qtObject, QT_THREAD_INTERRUPTED);
        } catch (IOException ex) {
            if (logEnabled)
                Log.d(TAG, "InputStream.read() failed:" + ex.toString());
            ex.printStackTrace();
            errorOccurred(qtObject, QT_READ_FAILED);
        }

        if (logEnabled)
            Log.d(TAG, "Leaving input stream thread");
    }

    public static native void errorOccurred(long qtObject, int errorCode);
    public static native void readyData(long qtObject, byte[] buffer, int bufferLength);
}
