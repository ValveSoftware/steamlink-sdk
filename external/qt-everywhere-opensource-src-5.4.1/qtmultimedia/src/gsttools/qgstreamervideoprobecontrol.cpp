/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#include "qgstreamervideoprobecontrol_p.h"
#include <private/qvideosurfacegstsink_p.h>
#include <private/qgstvideobuffer_p.h>

QGstreamerVideoProbeControl::QGstreamerVideoProbeControl(QObject *parent)
    : QMediaVideoProbeControl(parent)
    , m_flushing(false)
    , m_frameProbed(false)
{

}

QGstreamerVideoProbeControl::~QGstreamerVideoProbeControl()
{

}

void QGstreamerVideoProbeControl::startFlushing()
{
    m_flushing = true;

    {
        QMutexLocker locker(&m_frameMutex);
        m_pendingFrame = QVideoFrame();
    }

    // only emit flush if at least one frame was probed
    if (m_frameProbed)
        emit flush();
}

void QGstreamerVideoProbeControl::stopFlushing()
{
    m_flushing = false;
}

void QGstreamerVideoProbeControl::bufferProbed(GstBuffer* buffer)
{
    if (m_flushing)
        return;

    GstCaps* caps = gst_buffer_get_caps(buffer);
    if (!caps)
        return;

    int bytesPerLine = 0;
    QVideoSurfaceFormat format = QVideoSurfaceGstSink::formatForCaps(caps, &bytesPerLine);
    gst_caps_unref(caps);
    if (!format.isValid() || !bytesPerLine)
        return;

    QVideoFrame frame = QVideoFrame(new QGstVideoBuffer(buffer, bytesPerLine),
                                    format.frameSize(), format.pixelFormat());

    QVideoSurfaceGstSink::setFrameTimeStamps(&frame, buffer);

    m_frameProbed = true;

    {
        QMutexLocker locker(&m_frameMutex);
        m_pendingFrame = frame;
        QMetaObject::invokeMethod(this, "frameProbed", Qt::QueuedConnection);
    }
}

void QGstreamerVideoProbeControl::frameProbed()
{
    QVideoFrame frame;
    {
        QMutexLocker locker(&m_frameMutex);
        if (!m_pendingFrame.isValid())
            return;
        frame = m_pendingFrame;
    }
    emit videoFrameProbed(frame);
}
