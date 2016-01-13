/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "avfvideorenderercontrol.h"
#include "avfcamerasession.h"
#include "avfcameraservice.h"
#include "avfcameradebug.h"

#include <QtMultimedia/qabstractvideosurface.h>
#include <QtMultimedia/qabstractvideobuffer.h>
#include <QtMultimedia/qvideosurfaceformat.h>

QT_USE_NAMESPACE

class CVPixelBufferVideoBuffer : public QAbstractVideoBuffer
{
public:
    CVPixelBufferVideoBuffer(CVPixelBufferRef buffer)
        : QAbstractVideoBuffer(NoHandle)
        , m_buffer(buffer)
        , m_mode(NotMapped)
    {
        CVPixelBufferRetain(m_buffer);
    }

    virtual ~CVPixelBufferVideoBuffer()
    {
        CVPixelBufferRelease(m_buffer);
    }

    MapMode mapMode() const { return m_mode; }

    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine)
    {
        if (mode != NotMapped && m_mode == NotMapped) {
            CVPixelBufferLockBaseAddress(m_buffer, 0);

            if (numBytes)
                *numBytes = CVPixelBufferGetDataSize(m_buffer);

            if (bytesPerLine)
                *bytesPerLine = CVPixelBufferGetBytesPerRow(m_buffer);

            m_mode = mode;

            return (uchar*)CVPixelBufferGetBaseAddress(m_buffer);
        } else {
            return 0;
        }
    }

    void unmap()
    {
        if (m_mode != NotMapped) {
            m_mode = NotMapped;
            CVPixelBufferUnlockBaseAddress(m_buffer, 0);
        }
    }

private:
    CVPixelBufferRef m_buffer;
    MapMode m_mode;
};

@interface AVFCaptureFramesDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
@private
    AVFVideoRendererControl *m_renderer;
}

- (AVFCaptureFramesDelegate *) initWithRenderer:(AVFVideoRendererControl*)renderer;

- (void) captureOutput:(AVCaptureOutput *)captureOutput
         didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
         fromConnection:(AVCaptureConnection *)connection;
@end

@implementation AVFCaptureFramesDelegate

- (AVFCaptureFramesDelegate *) initWithRenderer:(AVFVideoRendererControl*)renderer
{
    if (!(self = [super init]))
        return nil;

    self->m_renderer = renderer;
    return self;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
         didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
         fromConnection:(AVCaptureConnection *)connection
{
    Q_UNUSED(connection);
    Q_UNUSED(captureOutput);

    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);

    int width = CVPixelBufferGetWidth(imageBuffer);
    int height = CVPixelBufferGetHeight(imageBuffer);

    QAbstractVideoBuffer *buffer = new CVPixelBufferVideoBuffer(imageBuffer);
    QVideoFrame frame(buffer, QSize(width, height), QVideoFrame::Format_RGB32);
    m_renderer->syncHandleViewfinderFrame(frame);
}
@end


AVFVideoRendererControl::AVFVideoRendererControl(QObject *parent)
   : QVideoRendererControl(parent)
   , m_surface(0)
   , m_needsHorizontalMirroring(false)
{
    m_viewfinderFramesDelegate = [[AVFCaptureFramesDelegate alloc] initWithRenderer:this];
}

AVFVideoRendererControl::~AVFVideoRendererControl()
{
    [m_cameraSession->captureSession() removeOutput:m_videoDataOutput];
    [m_viewfinderFramesDelegate release];
}

QAbstractVideoSurface *AVFVideoRendererControl::surface() const
{
    return m_surface;
}

void AVFVideoRendererControl::setSurface(QAbstractVideoSurface *surface)
{
    if (m_surface != surface) {
        m_surface = surface;
        Q_EMIT surfaceChanged(surface);
    }
}

void AVFVideoRendererControl::configureAVCaptureSession(AVFCameraSession *cameraSession)
{
    m_cameraSession = cameraSession;
    connect(m_cameraSession, SIGNAL(readyToConfigureConnections()),
            this, SLOT(updateCaptureConnection()));

    m_needsHorizontalMirroring = false;

    m_videoDataOutput = [[[AVCaptureVideoDataOutput alloc] init] autorelease];

    // Configure video output
    dispatch_queue_t queue = dispatch_queue_create("vf_queue", NULL);
    [m_videoDataOutput
            setSampleBufferDelegate:m_viewfinderFramesDelegate
            queue:queue];
    dispatch_release(queue);

    // Specify the pixel format
    m_videoDataOutput.videoSettings =
            [NSDictionary dictionaryWithObject:
            [NSNumber numberWithInt:kCVPixelFormatType_32BGRA]
            forKey:(id)kCVPixelBufferPixelFormatTypeKey];

    [m_cameraSession->captureSession() addOutput:m_videoDataOutput];
}

void AVFVideoRendererControl::updateCaptureConnection()
{
    AVCaptureConnection *connection = [m_videoDataOutput connectionWithMediaType:AVMediaTypeVideo];
    if (connection == nil || !m_cameraSession->videoCaptureDevice())
        return;

    // Frames of front-facing cameras should be mirrored horizontally (it's the default when using
    // AVCaptureVideoPreviewLayer but not with AVCaptureVideoDataOutput)
    if (connection.isVideoMirroringSupported)
        connection.videoMirrored = m_cameraSession->videoCaptureDevice().position == AVCaptureDevicePositionFront;

    // If the connection does't support mirroring, we'll have to do it ourselves
    m_needsHorizontalMirroring = !connection.isVideoMirrored
            && m_cameraSession->videoCaptureDevice().position == AVCaptureDevicePositionFront;
}

//can be called from non main thread
void AVFVideoRendererControl::syncHandleViewfinderFrame(const QVideoFrame &frame)
{
    QMutexLocker lock(&m_vfMutex);
    if (!m_lastViewfinderFrame.isValid()) {
        static QMetaMethod handleViewfinderFrameSlot = metaObject()->method(
                    metaObject()->indexOfMethod("handleViewfinderFrame()"));

        handleViewfinderFrameSlot.invoke(this, Qt::QueuedConnection);
    }

    m_lastViewfinderFrame = frame;

    if (m_needsHorizontalMirroring) {
        m_lastViewfinderFrame.map(QAbstractVideoBuffer::ReadOnly);

        // no deep copy
        QImage image(m_lastViewfinderFrame.bits(),
                     m_lastViewfinderFrame.size().width(),
                     m_lastViewfinderFrame.size().height(),
                     m_lastViewfinderFrame.bytesPerLine(),
                     QImage::Format_RGB32);

        QImage mirrored = image.mirrored(true, false);

        m_lastViewfinderFrame.unmap();
        m_lastViewfinderFrame = QVideoFrame(mirrored);
    }
}

void AVFVideoRendererControl::handleViewfinderFrame()
{
    QVideoFrame frame;
    {
        QMutexLocker lock(&m_vfMutex);
        frame = m_lastViewfinderFrame;
        m_lastViewfinderFrame = QVideoFrame();
    }

    if (m_surface && frame.isValid()) {
        if (m_surface->isActive() && m_surface->surfaceFormat().pixelFormat() != frame.pixelFormat())
            m_surface->stop();

        if (!m_surface->isActive()) {
            QVideoSurfaceFormat format(frame.size(), frame.pixelFormat());

            if (!m_surface->start(format)) {
                qWarning() << "Failed to start viewfinder m_surface, format:" << format;
            } else {
                qDebugCamera() << "Viewfinder started: " << format;
            }
        }

        if (m_surface->isActive())
            m_surface->present(frame);
    }
}


#include "moc_avfvideorenderercontrol.cpp"
