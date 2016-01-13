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

import android.hardware.Camera;
import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.util.Log;
import java.lang.Math;
import java.util.concurrent.locks.ReentrantLock;

public class QtCameraListener implements Camera.ShutterCallback,
                                         Camera.PictureCallback,
                                         Camera.AutoFocusCallback,
                                         Camera.PreviewCallback
{
    private int m_cameraId = -1;
    private byte[][] m_cameraPreviewBuffer = null;
    private volatile int m_actualPreviewBuffer = 0;
    private final ReentrantLock m_buffersLock = new ReentrantLock();
    private boolean m_fetchEachFrame = false;

    private static final String TAG = "Qt Camera";

    private QtCameraListener(int id)
    {
        m_cameraId = id;
    }

    public void preparePreviewBuffer(Camera camera)
    {
        Camera.Size previewSize = camera.getParameters().getPreviewSize();
        double bytesPerPixel = ImageFormat.getBitsPerPixel(camera.getParameters().getPreviewFormat()) / 8.0;
        int bufferSizeNeeded = (int)Math.ceil(bytesPerPixel*previewSize.width*previewSize.height);
        m_buffersLock.lock();
        if (m_cameraPreviewBuffer == null || m_cameraPreviewBuffer[0].length < bufferSizeNeeded)
            m_cameraPreviewBuffer = new byte[2][bufferSizeNeeded];
        m_buffersLock.unlock();
    }

    public void fetchEachFrame(boolean fetch)
    {
        m_fetchEachFrame = fetch;
    }

    public byte[] lockAndFetchPreviewBuffer()
    {
        //This method should always be followed by unlockPreviewBuffer()
        //This method is not just a getter. It also marks last preview as already seen one.
        //We should reset actualBuffer flag here to make sure we will not use old preview with future captures
        byte[] result = null;
        m_buffersLock.lock();
        result = m_cameraPreviewBuffer[(m_actualPreviewBuffer == 1) ? 0 : 1];
        m_actualPreviewBuffer = 0;
        return result;
    }

    public void unlockPreviewBuffer()
    {
        if (m_buffersLock.isHeldByCurrentThread())
            m_buffersLock.unlock();
    }

    public byte[] callbackBuffer()
    {
        return m_cameraPreviewBuffer[(m_actualPreviewBuffer == 1) ? 1 : 0];
    }

    @Override
    public void onShutter()
    {
        notifyPictureExposed(m_cameraId);
    }

    @Override
    public void onPictureTaken(byte[] data, Camera camera)
    {
        notifyPictureCaptured(m_cameraId, data);
    }

    @Override
    public void onPreviewFrame(byte[] data, Camera camera)
    {
        m_buffersLock.lock();

        if (data != null && m_fetchEachFrame)
            notifyFrameFetched(m_cameraId, data);

        if (data == m_cameraPreviewBuffer[0])
            m_actualPreviewBuffer = 1;
        else if (data == m_cameraPreviewBuffer[1])
            m_actualPreviewBuffer = 2;
        else
            m_actualPreviewBuffer = 0;
        camera.addCallbackBuffer(m_cameraPreviewBuffer[(m_actualPreviewBuffer == 1) ? 1 : 0]);
        m_buffersLock.unlock();
    }

    @Override
    public void onAutoFocus(boolean success, Camera camera)
    {
        notifyAutoFocusComplete(m_cameraId, success);
    }

    private static native void notifyAutoFocusComplete(int id, boolean success);
    private static native void notifyPictureExposed(int id);
    private static native void notifyPictureCaptured(int id, byte[] data);
    private static native void notifyFrameFetched(int id, byte[] data);
}
