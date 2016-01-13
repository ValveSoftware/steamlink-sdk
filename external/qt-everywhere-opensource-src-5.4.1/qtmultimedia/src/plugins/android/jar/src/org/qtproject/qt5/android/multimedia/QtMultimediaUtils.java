/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtMultimedia of the Qt Toolkit.
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

package org.qtproject.qt5.android.multimedia;

import android.content.Context;
import android.app.Activity;
import android.view.OrientationEventListener;
import android.os.Environment;
import android.media.MediaScannerConnection;
import java.lang.String;
import java.io.File;

public class QtMultimediaUtils
{
    static private class OrientationListener extends OrientationEventListener
    {
        static public int deviceOrientation = 0;

        public OrientationListener(Context context)
        {
            super(context);
        }

        @Override
        public void onOrientationChanged(int orientation)
        {
            if (orientation == ORIENTATION_UNKNOWN)
                return;

            deviceOrientation = orientation;
        }
    }

    static private Activity m_activity = null;
    static private OrientationListener m_orientationListener = null;

    static public void setActivity(Activity activity, Object activityDelegate)
    {
        m_activity = activity;
        m_orientationListener = new OrientationListener(activity);
    }

    public QtMultimediaUtils()
    {
    }

    static void enableOrientationListener(boolean enable)
    {
        if (enable)
            m_orientationListener.enable();
        else
            m_orientationListener.disable();
    }

    static int getDeviceOrientation()
    {
        return m_orientationListener.deviceOrientation;
    }

    static String getDefaultMediaDirectory(int type)
    {
        String dirType = new String();
        switch (type) {
            case 0:
                dirType = Environment.DIRECTORY_MUSIC;
                break;
            case 1:
                dirType = Environment.DIRECTORY_MOVIES;
                break;
            case 2:
                dirType = Environment.DIRECTORY_DCIM;
                break;
            default:
                break;
        }

        File path = new File("");
        if (type == 3) {
            // There is no API for knowing the standard location for sounds
            // such as voice recording. Though, it's typically in the 'Sounds'
            // directory at the root of the external storage
            path = new File(Environment.getExternalStorageDirectory().getAbsolutePath()
                            + File.separator + "Sounds");
        } else {
            path = Environment.getExternalStoragePublicDirectory(dirType);
        }

        path.mkdirs(); // make sure the directory exists

        return path.getAbsolutePath();
    }

    static void registerMediaFile(String file)
    {
        MediaScannerConnection.scanFile(m_activity, new String[] { file }, null, null);
    }
}
