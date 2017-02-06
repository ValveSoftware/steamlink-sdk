/****************************************************************************
**
** Copyright (C) 2016 BogDan Vatra <bogdan@kde.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensor module of the Qt Toolkit.
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

package org.qtproject.qt5.android.sensors;

import java.util.HashSet;
import java.util.List;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.util.SparseArray;

import android.util.Log;

public class QtSensors implements SensorEventListener
{
    static final QtSensors m_sensorsListener = new QtSensors();
    static SensorManager m_sensorManager = null;
    static SparseArray<Sensor> m_registeredSensors = new SparseArray<Sensor>();
    static Object m_syncObject = new Object();
    static public void setContext(Context context)
    {
        try {
            m_sensorManager = (SensorManager)context.getSystemService(Context.SENSOR_SERVICE);
        } catch(Exception e) {
            e.printStackTrace();
        }
    }

    private static String getSensorDescription(int sensorType)
    {
        try {
            Sensor s  = m_sensorManager.getDefaultSensor(sensorType);
            if (s == null) {
                return null;
            }
            return s.getName() + " " + s.getVendor() + " v" + s.getVersion();
        } catch(Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    private static int[] getSensorList()
    {
        try {
            List<Sensor> list = m_sensorManager.getSensorList(Sensor.TYPE_ALL);
            HashSet<Integer> filteredList = new HashSet<Integer>();
            for (Sensor s : list)
                filteredList.add(s.getType());
            int retList[] = new int[filteredList.size()];
            int pos = 0;
            for (int type : filteredList)
                retList[pos++] = type;
            return retList;
        } catch(Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    private static float getSensorMaximumRange(int sensorType)
    {
        try {
            Sensor s = m_sensorManager.getDefaultSensor(sensorType);
            return s.getMaximumRange();
        } catch(Exception e) {
            e.printStackTrace();
        }
        return 0;
    }

    private static boolean registerSensor(int sensorType, int rate)
    {
        synchronized (m_syncObject) {
            try {
                Sensor s  = m_sensorManager.getDefaultSensor(sensorType);
                m_sensorManager.registerListener(m_sensorsListener, s, rate);
                m_registeredSensors.put(sensorType, s);
            } catch(Exception e) {
                e.printStackTrace();
                return false;
            }
        }
        return true;
    }

    private static boolean unregisterSensor(int sensorType)
    {
        synchronized (m_syncObject) {
            try {
                Sensor s = m_registeredSensors.get(sensorType);
                if (s != null) {
                    m_sensorManager.unregisterListener(m_sensorsListener, m_registeredSensors.get(sensorType));
                    m_registeredSensors.remove(sensorType);
                }
            } catch (Exception e) {
                e.printStackTrace();
                return false;
            }
        }
        return true;
    }

    private static float[] convertQuaternionToEuler(float[] rotationVector)
    {
        float matrix[] = new float[9];
        SensorManager.getRotationMatrixFromVector (matrix, rotationVector);
        float angles[] = new float[3];
        SensorManager.getOrientation (matrix, angles);
        return angles;
    }

    private static float[] mRotation = new float[9];
    private static float[] mOrientation = new float[3];
    private static float[] mAcc = new float[3];
    private static float[] mMag = new float[3];

    private static float getCompassAzimuth(float a0, float a1, float a2, float m0, float m1, float m2)
    {
        mAcc[0] = a0;
        mAcc[1] = a1;
        mAcc[2] = a2;
        mMag[0] = m0;
        mMag[1] = m1;
        mMag[2] = m2;

        SensorManager.getRotationMatrix(mRotation, null, mAcc, mMag);
        SensorManager.getOrientation(mRotation, mOrientation);
        return mOrientation[0];
    }

    public static native void accuracyChanged(int sensorType, int accuracy);
    public static native void sensorChanged(int sensorType, long timestamp, float[] values);

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy)
    {
        accuracyChanged(sensor.getType(), accuracy);
    }

    @Override
    public void onSensorChanged(SensorEvent sensorEvent)
    {
        if (sensorEvent.sensor.getType() == Sensor.TYPE_ROTATION_VECTOR) {
            //#### hacky, but much easier than exposing the convert function and converting the arrays back and forth...
            sensorChanged(sensorEvent.sensor.getType(), sensorEvent.timestamp, convertQuaternionToEuler(sensorEvent.values));
        } else {
            sensorChanged(sensorEvent.sensor.getType(), sensorEvent.timestamp, sensorEvent.values);
        }
    }
}
