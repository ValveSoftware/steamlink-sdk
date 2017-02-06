/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtLocation of the Qt Toolkit.
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

package org.qtproject.qt5.android.positioning;

import android.content.Context;
import android.location.GpsSatellite;
import android.location.GpsStatus;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;

import android.util.Log;

public class QtPositioning implements LocationListener
{

    private static final String TAG = "QtPositioning";
    static LocationManager locationManager = null;
    static Object m_syncObject = new Object();
    static HashMap<Integer, QtPositioning> runningListeners = new HashMap<Integer, QtPositioning>();

    /*
        The positionInfo instance to which this
        QtPositioning instance is attached to.
    */
    private int nativeClassReference = 0;

    /*
        The provider type requested by Qt
    */
    private int expectedProviders = 0;

    public static final int QT_GPS_PROVIDER = 1;
    public static final int QT_NETWORK_PROVIDER = 2;

    /* The following values must match the corresponding error enums in the Qt API*/
    public static final int QT_ACCESS_ERROR = 0;
    public static final int QT_CLOSED_ERROR = 1;
    public static final int QT_POSITION_UNKNOWN_SOURCE_ERROR = 2;
    public static final int QT_POSITION_NO_ERROR = 3;
    public static final int QT_SATELLITE_NO_ERROR = 2;
    public static final int QT_SATELLITE_UNKNOWN_SOURCE_ERROR = -1;

    /* True, if updates were caused by requestUpdate() */
    private boolean isSingleUpdate = false;
    /* The length requested for regular intervals in msec. */
    private int updateIntervalTime = 0;

    /* The last received GPS update */
    private Location lastGps = null;
    /* The last received network update */
    private Location lastNetwork = null;
    /* If true this class acts as satellite signal monitor rather than location monitor */
    private boolean isSatelliteUpdate = false;

    private PositioningLooper looperThread;

    static public void setContext(Context context)
    {
        try {
            locationManager = (LocationManager)context.getSystemService(Context.LOCATION_SERVICE);
        } catch(Exception e) {
            e.printStackTrace();
        }
    }

    static private int[] providerList()
    {
        List<String> providers = locationManager.getAllProviders();
        int retList[] = new int[providers.size()];
        for (int i = 0; i < providers.size();  i++) {
            if (providers.get(i).equals(LocationManager.GPS_PROVIDER)) {
                //must be in sync with AndroidPositioning::PositionProvider::PROVIDER_GPS
                retList[i] = 0;
            } else if (providers.get(i).equals(LocationManager.NETWORK_PROVIDER)) {
                //must be in sync with AndroidPositioning::PositionProvider::PROVIDER_NETWORK
                retList[i] = 1;
            } else if (providers.get(i).equals(LocationManager.PASSIVE_PROVIDER)) {
                //must be in sync with AndroidPositioning::PositionProvider::PROVIDER_PASSIVE
                retList[i] = 2;
            } else {
                retList[i] = -1;
            }
        }
        return retList;
    }

    static public Location lastKnownPosition(boolean fromSatelliteOnly)
    {
        Location gps = null;
        Location network = null;
        try {
            gps = locationManager.getLastKnownLocation(LocationManager.GPS_PROVIDER);
            if (!fromSatelliteOnly)
                network = locationManager.getLastKnownLocation(LocationManager.NETWORK_PROVIDER);
        } catch(Exception e) {
            e.printStackTrace();
            gps = network = null;
        }

        if (gps != null && network != null) {
            //we return the most recent location but slightly prefer GPS
            //prefer GPS if it is max 4 hrs older than network
            long delta = network.getTime() - gps.getTime();
            if (delta < 4*60*60*1000) {
                return gps;
            } else {
                return network;
            }
        } else if (gps != null ) {
            return gps;
        } else if (network != null) {
            return network;
        }

        return null;
    }

    /* Returns true if at least on of the given providers is enabled. */
    static private boolean expectedProvidersAvailable(int desiredProviders)
    {
        List<String> enabledProviders = locationManager.getProviders(true);
        if ((desiredProviders & QT_GPS_PROVIDER) > 0) { //gps desired
            if (enabledProviders.contains(LocationManager.GPS_PROVIDER)) {
                return true;
            }
        }
        if ((desiredProviders & QT_NETWORK_PROVIDER) > 0) { //network desired
            if (enabledProviders.contains(LocationManager.NETWORK_PROVIDER)) {
                return true;
            }
        }

        return false;
    }


    static private void addActiveListener(QtPositioning listener, String provider)
    {
        int androidClassKey = listener.nativeClassReference;
        //start update thread
        listener.setActiveLooper(true);

        if (runningListeners.containsKey(androidClassKey) && runningListeners.get(androidClassKey) != listener) {
            removeActiveListener(androidClassKey);
        }

        locationManager.requestSingleUpdate(provider,
                                            listener,
                                            listener.looper());

        runningListeners.put(androidClassKey, listener);
    }


    static private void addActiveListener(QtPositioning listener, String provider, long minTime, float minDistance)
    {
        int androidClassKey = listener.nativeClassReference;
        //start update thread
        listener.setActiveLooper(true);

        if (runningListeners.containsKey(androidClassKey) && runningListeners.get(androidClassKey) != listener) {
            removeActiveListener(androidClassKey);
        }

        locationManager.requestLocationUpdates(provider,
                                               minTime, minDistance,
                                               listener,
                                               listener.looper());

        runningListeners.put(androidClassKey, listener);
    }


    static private void removeActiveListener(QtPositioning listener)
    {
        removeActiveListener(listener.nativeClassReference);
    }


    static private void removeActiveListener(int androidClassKey)
    {
        QtPositioning listener = runningListeners.remove(androidClassKey);
        locationManager.removeUpdates(listener);
        listener.setActiveLooper(false);
    }


    static public int startUpdates(int androidClassKey, int locationProvider, int updateInterval)
    {
        synchronized (m_syncObject) {
            try {
                boolean exceptionOccurred = false;
                QtPositioning positioningListener = new QtPositioning();
                positioningListener.nativeClassReference = androidClassKey;
                positioningListener.expectedProviders = locationProvider;
                positioningListener.isSatelliteUpdate = false;

                if (updateInterval == 0)
                    updateInterval = 50; //don't update more often than once per 50ms

                positioningListener.updateIntervalTime = updateInterval;
                if ((locationProvider & QT_GPS_PROVIDER) > 0) {
                    Log.d(TAG, "Regular updates using GPS " + updateInterval);
                    try {
                        addActiveListener(positioningListener,
                                        LocationManager.GPS_PROVIDER,
                                        updateInterval, 0);
                    } catch (SecurityException se) {
                        se.printStackTrace();
                        exceptionOccurred = true;
                    }
                }

                if ((locationProvider & QT_NETWORK_PROVIDER) > 0) {
                    Log.d(TAG, "Regular updates using network " + updateInterval);
                    try {
                        addActiveListener(positioningListener,
                                            LocationManager.NETWORK_PROVIDER,
                                            updateInterval, 0);
                   } catch (SecurityException se) {
                       se.printStackTrace();
                       exceptionOccurred = true;
                   }
                }
                if (exceptionOccurred) {
                    removeActiveListener(positioningListener);
                    return QT_ACCESS_ERROR;
                }

                if (!expectedProvidersAvailable(locationProvider)) {
                    //all location providers unavailbe -> when they come back we resume automatically
                    return QT_CLOSED_ERROR;
                }

            } catch(Exception e) {
                e.printStackTrace();
                return QT_POSITION_UNKNOWN_SOURCE_ERROR;
            }

            return QT_POSITION_NO_ERROR;
        }
    }

    static public void stopUpdates(int androidClassKey)
    {
        synchronized (m_syncObject) {
            try {
                Log.d(TAG, "Stopping updates");
                removeActiveListener(androidClassKey);
            } catch(Exception e) {
                e.printStackTrace();
                return;
            }
        }
    }

    static public int requestUpdate(int androidClassKey, int locationProvider)
    {
        synchronized (m_syncObject) {
            try {
                boolean exceptionOccurred = false;
                QtPositioning positioningListener = new QtPositioning();
                positioningListener.nativeClassReference = androidClassKey;
                positioningListener.isSingleUpdate = true;
                positioningListener.expectedProviders = locationProvider;
                positioningListener.isSatelliteUpdate = false;

                if ((locationProvider & QT_GPS_PROVIDER) > 0) {
                    Log.d(TAG, "Single update using GPS");
                    try {
                        addActiveListener(positioningListener, LocationManager.GPS_PROVIDER);
                    } catch (SecurityException se) {
                        se.printStackTrace();
                        exceptionOccurred = true;
                    }
                }

                if ((locationProvider & QT_NETWORK_PROVIDER) > 0) {
                    Log.d(TAG, "Single update using network");
                    try {
                        addActiveListener(positioningListener, LocationManager.NETWORK_PROVIDER);
                    } catch (SecurityException se) {
                         se.printStackTrace();
                         exceptionOccurred = true;
                    }
                }
                if (exceptionOccurred) {
                    removeActiveListener(positioningListener);
                    return QT_ACCESS_ERROR;
                }

                if (!expectedProvidersAvailable(locationProvider)) {
                    //all location providers unavailable -> when they come back we resume automatically
                    //in the mean time return ClosedError
                    return QT_CLOSED_ERROR;
                }

            } catch(Exception e) {
                e.printStackTrace();
                return QT_POSITION_UNKNOWN_SOURCE_ERROR;
            }

            return QT_POSITION_NO_ERROR;
        }
    }

    static public int startSatelliteUpdates(int androidClassKey, int updateInterval, boolean isSingleRequest)
    {
        synchronized (m_syncObject) {
            try {
                boolean exceptionOccurred = false;
                QtPositioning positioningListener = new QtPositioning();
                positioningListener.isSatelliteUpdate = true;
                positioningListener.nativeClassReference = androidClassKey;
                positioningListener.expectedProviders = 1; //always satellite provider
                positioningListener.isSingleUpdate = isSingleRequest;

                if (updateInterval == 0)
                    updateInterval = 50; //don't update more often than once per 50ms

                if (isSingleRequest)
                    Log.d(TAG, "Single update for Satellites " + updateInterval);
                else
                    Log.d(TAG, "Regular updates for Satellites " + updateInterval);
                try {
                    addActiveListener(positioningListener, LocationManager.GPS_PROVIDER,
                                        updateInterval, 0);
                } catch (SecurityException se) {
                    se.printStackTrace();
                    exceptionOccurred = true;
                }

                if (exceptionOccurred) {
                    removeActiveListener(positioningListener);
                    return QT_ACCESS_ERROR;
                }

                if (!expectedProvidersAvailable(positioningListener.expectedProviders)) {
                    //all location providers unavailable -> when they come back we resume automatically
                    //in the mean time return ClosedError
                    return QT_CLOSED_ERROR;
                }

            } catch(Exception e) {
                e.printStackTrace();
                return QT_SATELLITE_UNKNOWN_SOURCE_ERROR;
            }

            return QT_SATELLITE_NO_ERROR;
        }
    }

    public QtPositioning()
    {
        looperThread = new PositioningLooper();
    }

    public Looper looper()
    {
        return looperThread.looper();
    }

    private void setActiveLooper(boolean setActive)
    {
        try{
            if (setActive) {
                if (looperThread.isAlive())
                    return;

                if (isSatelliteUpdate)
                    looperThread.isSatelliteListener(true);

                long start = System.currentTimeMillis();
                looperThread.start();

                //busy wait but lasts ~20-30 ms only
                while (!looperThread.isReady());

                long stop = System.currentTimeMillis();
                Log.d(TAG, "Looper Thread startup time in ms: " + (stop-start));
            } else {
                looperThread.quitLooper();
            }
        } catch(Exception e) {
            e.printStackTrace();
        }
    }

    private class PositioningLooper extends Thread implements GpsStatus.Listener{
        private boolean looperRunning;
        private Looper posLooper;
        private boolean isSatelliteLooper = false;
        private LocationManager locManager = null;

        private PositioningLooper()
        {
            looperRunning = false;
        }

        public void run()
        {
            Looper.prepare();
            Handler handler = new Handler();

            if (isSatelliteLooper) {
                try {
                    locationManager.addGpsStatusListener(this);
                } catch(Exception e) {
                    e.printStackTrace();
                }
            }

            posLooper = Looper.myLooper();
            synchronized (this) {
                looperRunning = true;
            }
            Looper.loop();
            synchronized (this) {
                looperRunning = false;
            }
        }

        public void quitLooper()
        {
            if (isSatelliteLooper)
                locationManager.removeGpsStatusListener(this);
            looper().quit();
        }

        public synchronized boolean isReady()
        {
            return looperRunning;
        }

        public void isSatelliteListener(boolean isListener)
        {
            isSatelliteLooper = isListener;
        }

        public Looper looper()
        {
            return posLooper;
        }

        @Override
        public void onGpsStatusChanged(int event) {
            switch (event) {
                case GpsStatus.GPS_EVENT_FIRST_FIX:
                    break;
                case GpsStatus.GPS_EVENT_SATELLITE_STATUS:
                    GpsStatus status = locationManager.getGpsStatus(null);
                    Iterable<GpsSatellite> iterable = status.getSatellites();
                    Iterator<GpsSatellite> it = iterable.iterator();

                    ArrayList<GpsSatellite> list = new ArrayList<GpsSatellite>();
                    while (it.hasNext()) {
                        GpsSatellite sat = (GpsSatellite) it.next();
                        list.add(sat);
                    }
                    GpsSatellite[] sats = list.toArray(new GpsSatellite[list.size()]);
                    satelliteUpdated(sats, nativeClassReference, isSingleUpdate);

                    break;
                case GpsStatus.GPS_EVENT_STARTED:
                    break;
                case GpsStatus.GPS_EVENT_STOPPED:
                    break;
            }
        }
    }



    public static native void positionUpdated(Location update, int androidClassKey, boolean isSingleUpdate);
    public static native void locationProvidersDisabled(int androidClassKey);
    public static native void satelliteUpdated(GpsSatellite[] update, int androidClassKey, boolean isSingleUpdate);

    @Override
    public void onLocationChanged(Location location) {
        //Log.d(TAG, "**** Position Update ****: " +  location.toString() + " " + isSingleUpdate);
        if (location == null)
            return;

        if (isSatelliteUpdate) //we are a QGeoSatelliteInfoSource -> ignore
            return;

        if (isSingleUpdate || expectedProviders < 3) {
            positionUpdated(location, nativeClassReference, isSingleUpdate);
            return;
        }

        /*
            We can use GPS and Network, pick the better location provider.
            Generally we prefer GPS data due to their higher accurancy but we
            let Network data pass until GPS fix is available
        */

        if (location.getProvider().equals(LocationManager.GPS_PROVIDER)) {
            lastGps = location;

            // assumption: GPS always better -> pass it on
            positionUpdated(location, nativeClassReference, isSingleUpdate);
        } else if (location.getProvider().equals(LocationManager.NETWORK_PROVIDER)) {
            lastNetwork = location;

            if (lastGps == null) { //no GPS fix yet use network location
                positionUpdated(location, nativeClassReference, isSingleUpdate);
                return;
            }

            long delta = location.getTime() - lastGps.getTime();

            // Ignore if network update is older than last GPS (delta < 0)
            // Ignore if gps update still has time to provide next location (delta < updateInterval)
            if (delta < updateIntervalTime)
                return;

            // Use network data -> GPS has timed out on updateInterval
            positionUpdated(location, nativeClassReference, isSingleUpdate);
        }
    }

    @Override
    public void onStatusChanged(String provider, int status, Bundle extras) {}

    @Override
    public void onProviderEnabled(String provider) {
        Log.d(TAG, "Enabled provider: " + provider);
    }

    @Override
    public void onProviderDisabled(String provider) {
        Log.d(TAG, "Disabled provider: " + provider);
        if (!expectedProvidersAvailable(expectedProviders))
            locationProvidersDisabled(nativeClassReference);
    }
}
