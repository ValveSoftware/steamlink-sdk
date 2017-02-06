/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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


#ifndef CAMERABINIMAGECAPTURECONTROL_H
#define CAMERABINIMAGECAPTURECONTROL_H

#include <qcameraimagecapturecontrol.h>
#include "camerabinsession.h"

#include <qvideosurfaceformat.h>

#include <private/qgstreamerbufferprobe_p.h>

#if GST_CHECK_VERSION(1,0,0)
#include <gst/video/video.h>
#endif

QT_BEGIN_NAMESPACE

class CameraBinImageCapture : public QCameraImageCaptureControl, public QGstreamerBusMessageFilter
{
    Q_OBJECT
    Q_INTERFACES(QGstreamerBusMessageFilter)
public:
    CameraBinImageCapture(CameraBinSession *session);
    virtual ~CameraBinImageCapture();

    QCameraImageCapture::DriveMode driveMode() const { return QCameraImageCapture::SingleImageCapture; }
    void setDriveMode(QCameraImageCapture::DriveMode) {}

    bool isReadyForCapture() const;
    int capture(const QString &fileName);
    void cancelCapture();

    bool processBusMessage(const QGstreamerMessage &message);

private slots:
    void updateState();

private:
#if GST_CHECK_VERSION(1,0,0)
    static GstPadProbeReturn encoderEventProbe(GstPad *, GstPadProbeInfo *info, gpointer user_data);
#else
    static gboolean encoderEventProbe(GstElement *, GstEvent *event, gpointer user_data);
#endif

    class EncoderProbe : public QGstreamerBufferProbe
    {
    public:
        EncoderProbe(CameraBinImageCapture *capture) : capture(capture) {}
        void probeCaps(GstCaps *caps);
        bool probeBuffer(GstBuffer *buffer);

    private:
        CameraBinImageCapture * const capture;
    } m_encoderProbe;

    class MuxerProbe : public QGstreamerBufferProbe
    {
    public:
        MuxerProbe(CameraBinImageCapture *capture) : capture(capture) {}
        void probeCaps(GstCaps *caps);
        bool probeBuffer(GstBuffer *buffer);

    private:
        CameraBinImageCapture * const capture;

    } m_muxerProbe;

    QVideoSurfaceFormat m_bufferFormat;
    QSize m_jpegResolution;
    CameraBinSession *m_session;
    GstElement *m_jpegEncoderElement;
    GstElement *m_metadataMuxerElement;
#if GST_CHECK_VERSION(1,0,0)
    GstVideoInfo m_videoInfo;
#else
    int m_bytesPerLine;
#endif
    int m_requestId;
    bool m_ready;
};

QT_END_NAMESPACE

#endif // CAMERABINCAPTURECORNTROL_H
