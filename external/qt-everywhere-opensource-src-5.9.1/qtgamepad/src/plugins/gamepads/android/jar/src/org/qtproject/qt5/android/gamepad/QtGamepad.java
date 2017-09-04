/****************************************************************************
**
** Copyright (C) 2015 BogDan Vatra <bogdan@kde.org>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Gamepad module
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

package org.qtproject.qt5.android.gamepad;

import android.app.Activity;
import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Handler;


public class QtGamepad {
    private long m_nativePtr = 0;
    private InputManager m_manager;
    Activity m_activity;
    private InputManager.InputDeviceListener m_listener = new InputManager.InputDeviceListener() {
        @Override
        public void onInputDeviceAdded(int i) {
            synchronized (QtGamepad.this) {
                QtGamepad.onInputDeviceAdded(m_nativePtr, i);
            }
        }

        @Override
        public void onInputDeviceRemoved(int i) {
            synchronized (QtGamepad.this) {
                QtGamepad.onInputDeviceRemoved(m_nativePtr, i);
            }
        }

        @Override
        public void onInputDeviceChanged(int i) {
            synchronized (QtGamepad.this) {
                QtGamepad.onInputDeviceChanged(m_nativePtr, i);
            }
        }
    };

    QtGamepad(Activity activity)
    {
        m_manager = (InputManager) activity.getSystemService(Context.INPUT_SERVICE);
        m_activity = activity;
    }

    public void register(long nativePtr)
    {
        synchronized (this) {
            if (m_manager != null) {
                m_nativePtr = nativePtr;
                m_activity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        try {
                            m_manager.registerInputDeviceListener(m_listener, new Handler());
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                    }
                });
            }
        }
    }

    public void unregister()
    {
        synchronized (this) {
            if (m_manager != null) {
                m_nativePtr = 0;
                m_manager.unregisterInputDeviceListener(m_listener);
            }
        }
    }

    private static native void onInputDeviceAdded(long nativePtr, int deviceId);
    private static native void onInputDeviceRemoved(long nativePtr, int deviceId);
    private static native void onInputDeviceChanged(long nativePtr, int deviceId);
}
