/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "avfcameraviewfindersettingscontrol.h"
#include "private/qabstractvideobuffer_p.h"
#include "avfcamerarenderercontrol.h"
#include "avfcamerasession.h"
#include "avfcameraservice.h"
#include "avfcameradebug.h"

#ifdef Q_OS_IOS
#include <QtGui/qopengl.h>
#endif

#include <QtMultimedia/qabstractvideosurface.h>
#include <QtMultimedia/qabstractvideobuffer.h>

#include <QtMultimedia/qvideosurfaceformat.h>

QT_USE_NAMESPACE

class CVImageVideoBuffer : public QAbstractPlanarVideoBuffer
{
public:
    CVImageVideoBuffer(CVImageBufferRef buffer, AVFCameraRendererControl *renderer)
#ifndef Q_OS_IOS
        : QAbstractPlanarVideoBuffer(NoHandle)
#else
        : QAbstractPlanarVideoBuffer(renderer->supportsTextures()
                                     && CVPixelBufferGetPixelFormatType(buffer) == kCVPixelFormatType_32BGRA
                                     ? GLTextureHandle : NoHandle)
        , m_texture(0)
        , m_renderer(renderer)
#endif
        , m_buffer(buffer)
        , m_mode(NotMapped)
    {
#ifndef Q_OS_IOS
        Q_UNUSED(renderer)
#endif // Q_OS_IOS
        CVPixelBufferRetain(m_buffer);
    }

    ~CVImageVideoBuffer()
    {
        CVImageVideoBuffer::unmap();
#ifdef Q_OS_IOS
        if (m_texture)
            CFRelease(m_texture);
#endif
        CVPixelBufferRelease(m_buffer);
    }

    MapMode mapMode() const { return m_mode; }

    int map(QAbstractVideoBuffer::MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4])
    {
        // We only support RGBA or NV12 (or Apple's version of NV12),
        // they are either 0 planes or 2.
        const size_t nPlanes = CVPixelBufferGetPlaneCount(m_buffer);
        Q_ASSERT(nPlanes <= 2);

        if (!nPlanes) {
            data[0] = map(mode, numBytes, bytesPerLine);
            return data[0] ? 1 : 0;
        }

        // For a bi-planar format we have to set the parameters correctly:
        if (mode != QAbstractVideoBuffer::NotMapped && m_mode == QAbstractVideoBuffer::NotMapped) {
            CVPixelBufferLockBaseAddress(m_buffer, mode == QAbstractVideoBuffer::ReadOnly
                                                               ? kCVPixelBufferLock_ReadOnly
                                                               : 0);

            if (numBytes)
                *numBytes = CVPixelBufferGetDataSize(m_buffer);

            if (bytesPerLine) {
                // At the moment we handle only bi-planar format.
                bytesPerLine[0] = CVPixelBufferGetBytesPerRowOfPlane(m_buffer, 0);
                bytesPerLine[1] = CVPixelBufferGetBytesPerRowOfPlane(m_buffer, 1);
            }

            if (data) {
                data[0] = (uchar *)CVPixelBufferGetBaseAddressOfPlane(m_buffer, 0);
                data[1] = (uchar *)CVPixelBufferGetBaseAddressOfPlane(m_buffer, 1);
            }

            m_mode = mode;
        }

        return nPlanes;
    }

    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine)
    {
        if (mode != NotMapped && m_mode == NotMapped) {
            CVPixelBufferLockBaseAddress(m_buffer, mode == QAbstractVideoBuffer::ReadOnly
                                                               ? kCVPixelBufferLock_ReadOnly
                                                               : 0);
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
            CVPixelBufferUnlockBaseAddress(m_buffer, m_mode == QAbstractVideoBuffer::ReadOnly
                                                                   ? kCVPixelBufferLock_ReadOnly
                                                                   : 0);
            m_mode = NotMapped;
        }
    }

    QVariant handle() const
    {
#ifdef Q_OS_IOS
        // Called from the render thread, so there is a current OpenGL context

        if (!m_renderer->m_textureCache) {
            CVReturn err = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault,
                                                        NULL,
                                                        [EAGLContext currentContext],
                                                        NULL,
                                                        &m_renderer->m_textureCache);

            if (err != kCVReturnSuccess)
                qWarning("Error creating texture cache");
        }

        if (m_renderer->m_textureCache && !m_texture) {
            CVOpenGLESTextureCacheFlush(m_renderer->m_textureCache, 0);

            CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                        m_renderer->m_textureCache,
                                                                        m_buffer,
                                                                        NULL,
                                                                        GL_TEXTURE_2D,
                                                                        GL_RGBA,
                                                                        CVPixelBufferGetWidth(m_buffer),
                                                                        CVPixelBufferGetHeight(m_buffer),
                                                                        GL_RGBA,
                                                                        GL_UNSIGNED_BYTE,
                                                                        0,
                                                                        &m_texture);
            if (err != kCVReturnSuccess)
                qWarning("Error creating texture from buffer");
        }

        if (m_texture)
            return CVOpenGLESTextureGetName(m_texture);
        else
            return 0;
#else
        return QVariant();
#endif
    }

private:
#ifdef Q_OS_IOS
    mutable CVOpenGLESTextureRef m_texture;
    AVFCameraRendererControl *m_renderer;
#endif
    CVImageBufferRef m_buffer;
    MapMode m_mode;
};


@interface AVFCaptureFramesDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
@private
    AVFCameraRendererControl *m_renderer;
}

- (AVFCaptureFramesDelegate *) initWithRenderer:(AVFCameraRendererControl*)renderer;

- (void) captureOutput:(AVCaptureOutput *)captureOutput
         didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
         fromConnection:(AVCaptureConnection *)connection;

@end

@implementation AVFCaptureFramesDelegate

- (AVFCaptureFramesDelegate *) initWithRenderer:(AVFCameraRendererControl*)renderer
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

    // NB: on iOS captureOutput/connection can be nil (when recording a video -
    // avfmediaassetwriter).

    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);

    int width = CVPixelBufferGetWidth(imageBuffer);
    int height = CVPixelBufferGetHeight(imageBuffer);
    QVideoFrame::PixelFormat format =
            AVFCameraViewfinderSettingsControl2::QtPixelFormatFromCVFormat(CVPixelBufferGetPixelFormatType(imageBuffer));
    if (format == QVideoFrame::Format_Invalid)
        return;

    QVideoFrame frame(new CVImageVideoBuffer(imageBuffer, m_renderer),
                      QSize(width, height),
                      format);

    m_renderer->syncHandleViewfinderFrame(frame);
}

@end


AVFCameraRendererControl::AVFCameraRendererControl(QObject *parent)
   : QVideoRendererControl(parent)
   , m_surface(0)
   , m_supportsTextures(false)
   , m_needsHorizontalMirroring(false)
#ifdef Q_OS_IOS
   , m_textureCache(0)
#endif
{
    m_viewfinderFramesDelegate = [[AVFCaptureFramesDelegate alloc] initWithRenderer:this];
}

AVFCameraRendererControl::~AVFCameraRendererControl()
{
    [m_cameraSession->captureSession() removeOutput:m_videoDataOutput];
    [m_viewfinderFramesDelegate release];
    if (m_delegateQueue)
        dispatch_release(m_delegateQueue);
#ifdef Q_OS_IOS
    if (m_textureCache)
        CFRelease(m_textureCache);
#endif
}

QAbstractVideoSurface *AVFCameraRendererControl::surface() const
{
    return m_surface;
}

void AVFCameraRendererControl::setSurface(QAbstractVideoSurface *surface)
{
    if (m_surface != surface) {
        m_surface = surface;
#ifdef Q_OS_IOS
        m_supportsTextures = m_surface
                ? m_surface->supportedPixelFormats(QAbstractVideoBuffer::GLTextureHandle).contains(QVideoFrame::Format_BGRA32)
                : false;
#endif
        Q_EMIT surfaceChanged(surface);
    }
}

void AVFCameraRendererControl::configureAVCaptureSession(AVFCameraSession *cameraSession)
{
    m_cameraSession = cameraSession;
    connect(m_cameraSession, SIGNAL(readyToConfigureConnections()),
            this, SLOT(updateCaptureConnection()));

    m_needsHorizontalMirroring = false;

    m_videoDataOutput = [[[AVCaptureVideoDataOutput alloc] init] autorelease];

    // Configure video output
    m_delegateQueue = dispatch_queue_create("vf_queue", NULL);
    [m_videoDataOutput
            setSampleBufferDelegate:m_viewfinderFramesDelegate
            queue:m_delegateQueue];

    [m_cameraSession->captureSession() addOutput:m_videoDataOutput];
}

void AVFCameraRendererControl::updateCaptureConnection()
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
void AVFCameraRendererControl::syncHandleViewfinderFrame(const QVideoFrame &frame)
{
    QMutexLocker lock(&m_vfMutex);
    if (!m_lastViewfinderFrame.isValid()) {
        static QMetaMethod handleViewfinderFrameSlot = metaObject()->method(
                    metaObject()->indexOfMethod("handleViewfinderFrame()"));

        handleViewfinderFrameSlot.invoke(this, Qt::QueuedConnection);
    }

    m_lastViewfinderFrame = frame;

    if (m_cameraSession && m_lastViewfinderFrame.isValid())
        m_cameraSession->onCameraFrameFetched(m_lastViewfinderFrame);
}

AVCaptureVideoDataOutput *AVFCameraRendererControl::videoDataOutput() const
{
    return m_videoDataOutput;
}

#ifdef Q_OS_IOS

AVFCaptureFramesDelegate *AVFCameraRendererControl::captureDelegate() const
{
    return m_viewfinderFramesDelegate;
}

void AVFCameraRendererControl::resetCaptureDelegate() const
{
    [m_videoDataOutput setSampleBufferDelegate:m_viewfinderFramesDelegate queue:m_delegateQueue];
}

#endif

void AVFCameraRendererControl::handleViewfinderFrame()
{
    QVideoFrame frame;
    {
        QMutexLocker lock(&m_vfMutex);
        frame = m_lastViewfinderFrame;
        m_lastViewfinderFrame = QVideoFrame();
    }

    if (m_surface && frame.isValid()) {
        if (m_surface->isActive() && (m_surface->surfaceFormat().pixelFormat() != frame.pixelFormat()
                                      || m_surface->surfaceFormat().frameSize() != frame.size())) {
            m_surface->stop();
        }

        if (!m_surface->isActive()) {
            QVideoSurfaceFormat format(frame.size(), frame.pixelFormat(), frame.handleType());
            if (m_needsHorizontalMirroring)
                format.setProperty("mirrored", true);

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


#include "moc_avfcamerarenderercontrol.cpp"
