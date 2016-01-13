/****************************************************************************
**
** Copyright (C) 2013 BogDan Vatra <bogdan@kde.org>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensor module of the Qt Toolkit.
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

package org.qtproject.qt5.android.sensors;

import java.util.HashSet;
import java.util.List;

import android.app.Activity;
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
    static public void setActivity(Activity activity, Object acitvityDelegate)
    {
        try {
            m_sensorManager = (SensorManager)activity.getSystemService(Activity.SENSOR_SERVICE);
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
