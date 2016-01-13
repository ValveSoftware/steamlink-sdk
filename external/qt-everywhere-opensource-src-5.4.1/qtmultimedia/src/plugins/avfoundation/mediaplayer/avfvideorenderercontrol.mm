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
#include "avfdisplaylink.h"
#include "avfvideoframerenderer.h"

#include <QtMultimedia/qabstractvideobuffer.h>
#include <QtMultimedia/qabstractvideosurface.h>
#include <QtMultimedia/qvideosurfaceformat.h>
#include <QtCore/qdebug.h>

#import <AVFoundation/AVFoundation.h>

QT_USE_NAMESPACE

class TextureVideoBuffer : public QAbstractVideoBuffer
{
public:
    TextureVideoBuffer(GLuint textureId)
        : QAbstractVideoBuffer(GLTextureHandle)
        , m_textureId(textureId)
    {}

    virtual ~TextureVideoBuffer() {}

    MapMode mapMode() const { return NotMapped; }
    uchar *map(MapMode, int*, int*) { return 0; }
    void unmap() {}

    QVariant handle() const
    {
        return QVariant::fromValue<unsigned int>(m_textureId);
    }

private:
    GLuint m_textureId;
};

class QImageVideoBuffer : public QAbstractVideoBuffer
{
public:
    QImageVideoBuffer(const QImage &image)
        : QAbstractVideoBuffer(NoHandle)
        , m_image(image)
        , m_mode(NotMapped)
    {

    }

    MapMode mapMode() const { return m_mode; }
    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine)
    {
        if (mode != NotMapped && m_mode == NotMapped) {
            m_mode = mode;
            return m_image.bits();
        } else
            return 0;
    }

    void unmap() {
        m_mode = NotMapped;
    }
private:
    QImage m_image;
    MapMode m_mode;
};

AVFVideoRendererControl::AVFVideoRendererControl(QObject *parent)
    : QVideoRendererControl(parent)
    , m_surface(0)
    , m_playerLayer(0)
    , m_frameRenderer(0)
    , m_enableOpenGL(false)

{
    m_displayLink = new AVFDisplayLink(this);
    connect(m_displayLink, SIGNAL(tick(CVTimeStamp)), SLOT(updateVideoFrame(CVTimeStamp)));
}

AVFVideoRendererControl::~AVFVideoRendererControl()
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO;
#endif
    m_displayLink->stop();
    if (m_playerLayer)
        [(AVPlayerLayer*)m_playerLayer release];
}

QAbstractVideoSurface *AVFVideoRendererControl::surface() const
{
    return m_surface;
}

void AVFVideoRendererControl::setSurface(QAbstractVideoSurface *surface)
{
#ifdef QT_DEBUG_AVF
    qDebug() << "Set video surface" << surface;
#endif

    //When we have a valid surface, we can setup a frame renderer
    //and schedule surface updates with the display link.
    if (surface == m_surface)
        return;

    QMutexLocker locker(&m_mutex);

    if (m_surface && m_surface->isActive())
        m_surface->stop();

    m_surface = surface;

    //If the surface changed, then the current frame renderer is no longer valid
    if (m_frameRenderer)
        delete m_frameRenderer;

    //If there is now no surface to render too
    if (m_surface == 0) {
        m_displayLink->stop();
        return;
    }

    //Surface changed, so we need a new frame renderer
    m_frameRenderer = new AVFVideoFrameRenderer(m_surface, this);

    //Check for needed formats to render as OpenGL Texture
    m_enableOpenGL = m_surface->supportedPixelFormats(QAbstractVideoBuffer::GLTextureHandle).contains(QVideoFrame::Format_BGR32);

    //If we already have a layer, but changed surfaces start rendering again
    if (m_playerLayer && !m_displayLink->isActive()) {
        m_displayLink->start();
    }

}

void AVFVideoRendererControl::setLayer(void *playerLayer)
{
    if (m_playerLayer == playerLayer)
        return;

    [(AVPlayerLayer*)playerLayer retain];
    [(AVPlayerLayer*)m_playerLayer release];

    m_playerLayer = playerLayer;

    //If there is an active surface, make sure it has been stopped so that
    //we can update it's state with the new content.
    if (m_surface && m_surface->isActive())
        m_surface->stop();

    //If there is no layer to render, stop scheduling updates
    if (m_playerLayer == 0) {
        m_displayLink->stop();
        return;
    }

    setupVideoOutput();

    //If we now have both a valid surface and layer, start scheduling updates
    if (m_surface && !m_displayLink->isActive()) {
        m_displayLink->start();
    }
}

void AVFVideoRendererControl::updateVideoFrame(const CVTimeStamp &ts)
{
    Q_UNUSED(ts)

    AVPlayerLayer *playerLayer = (AVPlayerLayer*)m_playerLayer;

    if (!playerLayer) {
        qWarning("updateVideoFrame called without AVPlayerLayer (which shouldn't happen");
        return;
    }

    if (!playerLayer.readyForDisplay)
        return;

    if (m_enableOpenGL) {

        GLuint textureId = m_frameRenderer->renderLayerToTexture(playerLayer);

        //Make sure we got a valid texture
        if (textureId == 0) {
            qWarning("renderLayerToTexture failed");
            return;
        }

        QAbstractVideoBuffer *buffer = new TextureVideoBuffer(textureId);
        QVideoFrame frame = QVideoFrame(buffer, m_nativeSize, QVideoFrame::Format_BGR32);

        if (m_surface && frame.isValid()) {
            if (m_surface->isActive() && m_surface->surfaceFormat().pixelFormat() != frame.pixelFormat())
                m_surface->stop();

            if (!m_surface->isActive()) {
                QVideoSurfaceFormat format(frame.size(), frame.pixelFormat(), QAbstractVideoBuffer::GLTextureHandle);
                format.setScanLineDirection(QVideoSurfaceFormat::BottomToTop);

                if (!m_surface->start(format)) {
                    //Surface doesn't support GLTextureHandle
                    qWarning("Failed to activate video surface");
                }
            }

            if (m_surface->isActive())
                m_surface->present(frame);
        }
    } else {
        //fallback to rendering frames to QImages
        QImage frameData = m_frameRenderer->renderLayerToImage(playerLayer);

        if (frameData.isNull()) {
            qWarning("renterLayerToImage failed");
            return;
        }

        QAbstractVideoBuffer *buffer = new QImageVideoBuffer(frameData);
        QVideoFrame frame = QVideoFrame(buffer, m_nativeSize, QVideoFrame::Format_ARGB32_Premultiplied);

        if (m_surface && frame.isValid()) {
            if (m_surface->isActive() && m_surface->surfaceFormat().pixelFormat() != frame.pixelFormat())
                m_surface->stop();

            if (!m_surface->isActive()) {
                QVideoSurfaceFormat format(frame.size(), frame.pixelFormat(), QAbstractVideoBuffer::NoHandle);

                if (!m_surface->start(format)) {
                    qWarning("Failed to activate video surface");
                }
            }

            if (m_surface->isActive())
                m_surface->present(frame);
        }

    }
}

void AVFVideoRendererControl::setupVideoOutput()
{
    AVPlayerLayer *playerLayer = (AVPlayerLayer*)m_playerLayer;
    if (playerLayer)
        m_nativeSize = QSize(playerLayer.bounds.size.width, playerLayer.bounds.size.height);
}
