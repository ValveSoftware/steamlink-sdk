/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the (whatever) of the Qt Toolkit.
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

import android.view.SurfaceHolder;
import android.view.Surface;
import android.graphics.Rect;
import android.graphics.Canvas;

public class QtSurfaceTextureHolder implements SurfaceHolder
{
    private Surface surfaceTexture;

    public QtSurfaceTextureHolder(Surface surface)
    {
        surfaceTexture = surface;
    }

    @Override
    public void addCallback(SurfaceHolder.Callback callback)
    {
    }

    @Override
    public Surface getSurface()
    {
        return surfaceTexture;
    }

    @Override
    public Rect getSurfaceFrame()
    {
        return new Rect();
    }

    @Override
    public boolean isCreating()
    {
        return false;
    }

    @Override
    public Canvas lockCanvas(Rect dirty)
    {
        return new Canvas();
    }

    @Override
    public Canvas lockCanvas()
    {
        return new Canvas();
    }

    @Override
    public void removeCallback(SurfaceHolder.Callback callback)
    {
    }

    @Override
    public void setFixedSize(int width, int height)
    {
    }

    @Override
    public void setFormat(int format)
    {
    }

    @Override
    public void setKeepScreenOn(boolean screenOn)
    {
    }

    @Override
    public void setSizeFromLayout()
    {
    }

    @Override
    public void setType(int type)
    {
    }

    @Override
    public void unlockCanvasAndPost(Canvas canvas)
    {
    }
}
