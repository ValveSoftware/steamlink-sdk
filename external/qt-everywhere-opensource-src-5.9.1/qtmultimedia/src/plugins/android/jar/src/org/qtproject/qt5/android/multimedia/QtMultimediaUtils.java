/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtMultimedia of the Qt Toolkit.
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

package org.qtproject.qt5.android.multimedia;

import android.content.Context;
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

    static private Context m_context = null;
    static private OrientationListener m_orientationListener = null;

    static public void setContext(Context context)
    {
        m_context = context;
        m_orientationListener = new OrientationListener(context);
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
        MediaScannerConnection.scanFile(m_context, new String[] { file }, null, null);
    }
}
