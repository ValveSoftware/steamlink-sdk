/****************************************************************************
**
** Copyright (C) 2016 Centria research and development
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

package org.qtproject.qt5.android.nfc;

import java.lang.Thread;
import java.lang.Runnable;

import android.os.Parcelable;
import android.os.Looper;
import android.content.Context;
import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.IntentFilter;
import android.nfc.NfcAdapter;
import android.content.IntentFilter.MalformedMimeTypeException;
import android.os.Bundle;
import android.util.Log;
import android.content.BroadcastReceiver;

public class QtNfc
{
    /* static final QtNfc m_nfc = new QtNfc(); */
    static private final String TAG = "QtNfc";
    static public NfcAdapter m_adapter = null;
    static public PendingIntent m_pendingIntent = null;
    static public IntentFilter[] m_filters;
    static public Activity m_activity;

    static public void setContext(Context context)
    {
        if (!(context instanceof Activity)) {
            Log.w(TAG, "NFC only works with Android activities and not in Android services. " +
                       "NFC has been disabled.");
            return;
        }

        m_activity = (Activity)context;
        m_adapter = NfcAdapter.getDefaultAdapter(m_activity);
        if (m_adapter == null) {
            //Log.e(TAG, "No NFC available");
            return;
        }
        m_pendingIntent = PendingIntent.getActivity(
            m_activity,
            0,
            new Intent(m_activity, m_activity.getClass()).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
            0);

        //Log.d(TAG, "Pending intent:" + m_pendingIntent);

        IntentFilter filter = new IntentFilter(NfcAdapter.ACTION_NDEF_DISCOVERED);

        m_filters = new IntentFilter[]{
            filter
        };

        try {
            filter.addDataType("*/*");
        } catch(MalformedMimeTypeException e) {
            throw new RuntimeException("Fail", e);
        }

        //Log.d(TAG, "Thread:" + Thread.currentThread().getId());
    }

    static public boolean start()
    {
        if (m_adapter == null) return false;
        m_activity.runOnUiThread(new Runnable() {
            public void run() {
                //Log.d(TAG, "Enabling NFC");
                IntentFilter[] filters = new IntentFilter[2];
                filters[0] = new IntentFilter();
                filters[0].addAction(NfcAdapter.ACTION_NDEF_DISCOVERED);
                filters[0].addCategory(Intent.CATEGORY_DEFAULT);
                try {
                    filters[0].addDataType("*/*");
                } catch (MalformedMimeTypeException e) {
                    throw new RuntimeException("Check your mime type.");
                }
                // some tags will report as tech, even if they are ndef formated/formatable.
                filters[1] = new IntentFilter();
                filters[1].addAction(NfcAdapter.ACTION_TECH_DISCOVERED);
                String[][] techList = new String[][]{
                        {"android.nfc.tech.Ndef"},
                        {"android.nfc.tech.NdefFormatable"}
                    };
                try {
                    m_adapter.enableForegroundDispatch(m_activity, m_pendingIntent, filters, techList);
                } catch(IllegalStateException e) {
                    // On Android we must call enableForegroundDispatch when the activity is in foreground, only.
                    Log.d(TAG, "enableForegroundDispatch failed: " + e.toString());
                }
            }
        });
        return true;
    }

    static public boolean stop()
    {
        if (m_adapter == null) return false;
        m_activity.runOnUiThread(new Runnable() {
            public void run() {
                //Log.d(TAG, "Disabling NFC");
                try {
                    m_adapter.disableForegroundDispatch(m_activity);
                } catch(IllegalStateException e) {
                    // On Android we must call disableForegroundDispatch when the activity is in foreground, only.
                    Log.d(TAG, "disableForegroundDispatch failed: " + e.toString());
                }
            }
        });
        return true;
    }

    static public boolean isAvailable()
    {
        m_adapter = NfcAdapter.getDefaultAdapter(m_activity);
        if (m_adapter == null) {
            //Log.e(TAG, "No NFC available (Adapter is null)");
            return false;
        }
        return m_adapter.isEnabled();
    }

    static public Intent getStartIntent()
    {
        Log.d(TAG, "getStartIntent");
        if (m_activity == null) return null;
        Intent intent = m_activity.getIntent();
        if (NfcAdapter.ACTION_NDEF_DISCOVERED.equals(intent.getAction()) ||
                NfcAdapter.ACTION_TECH_DISCOVERED.equals(intent.getAction()) ||
                NfcAdapter.ACTION_TAG_DISCOVERED.equals(intent.getAction())) {
            return intent;
        } else {
            return null;
        }
    }
}
