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

#ifndef AVFVIDEOFRAMERENDERER_H
#define AVFVIDEOFRAMERENDERER_H

#include <QtCore/QObject>
#include <QtGui/QImage>
#include <QtGui/QOpenGLContext>
#include <QtCore/QSize>

@class AVPlayerLayer;
@class AVPlayerItemVideoOutput;

QT_BEGIN_NAMESPACE

class QOpenGLContext;
class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;
class QOffscreenSurface;
class QAbstractVideoSurface;

typedef struct __CVBuffer *CVBufferRef;
typedef CVBufferRef CVImageBufferRef;
typedef CVImageBufferRef CVPixelBufferRef;
#if defined(Q_OS_IOS) || defined(Q_OS_TVOS)
typedef struct __CVOpenGLESTextureCache *CVOpenGLESTextureCacheRef;
typedef CVImageBufferRef CVOpenGLESTextureRef;
// helpers to avoid boring if def
typedef CVOpenGLESTextureCacheRef CVOGLTextureCacheRef;
typedef CVOpenGLESTextureRef CVOGLTextureRef;
#define CVOGLTextureGetTarget CVOpenGLESTextureGetTarget
#define CVOGLTextureGetName CVOpenGLESTextureGetName
#define CVOGLTextureCacheCreate CVOpenGLESTextureCacheCreate
#define CVOGLTextureCacheCreateTextureFromImage CVOpenGLESTextureCacheCreateTextureFromImage
#define CVOGLTextureCacheFlush CVOpenGLESTextureCacheFlush
#else
typedef struct __CVOpenGLTextureCache *CVOpenGLTextureCacheRef;
typedef CVImageBufferRef CVOpenGLTextureRef;
// helpers to avoid boring if def
typedef CVOpenGLTextureCacheRef CVOGLTextureCacheRef;
typedef CVOpenGLTextureRef CVOGLTextureRef;
#define CVOGLTextureGetTarget CVOpenGLTextureGetTarget
#define CVOGLTextureGetName CVOpenGLTextureGetName
#define CVOGLTextureCacheCreate CVOpenGLTextureCacheCreate
#define CVOGLTextureCacheCreateTextureFromImage CVOpenGLTextureCacheCreateTextureFromImage
#define CVOGLTextureCacheFlush CVOpenGLTextureCacheFlush
#endif

class AVFVideoFrameRenderer : public QObject
{
public:
    AVFVideoFrameRenderer(QAbstractVideoSurface *surface, QObject *parent = 0);

    virtual ~AVFVideoFrameRenderer();

    void setPlayerLayer(AVPlayerLayer *layer);

    CVOGLTextureRef renderLayerToTexture(AVPlayerLayer *layer);
    QImage renderLayerToImage(AVPlayerLayer *layer);

private:
    void initRenderer();
    CVPixelBufferRef copyPixelBufferFromLayer(AVPlayerLayer *layer, size_t& width, size_t& height);
    CVOGLTextureRef createCacheTextureFromLayer(AVPlayerLayer *layer, size_t& width, size_t& height);

    QOpenGLContext *m_glContext;
    QOffscreenSurface *m_offscreenSurface;
    QAbstractVideoSurface *m_surface;
    CVOGLTextureCacheRef m_textureCache;
    AVPlayerItemVideoOutput* m_videoOutput;
    bool m_isContextShared;
};

QT_END_NAMESPACE

#endif // AVFVIDEOFRAMERENDERER_H
