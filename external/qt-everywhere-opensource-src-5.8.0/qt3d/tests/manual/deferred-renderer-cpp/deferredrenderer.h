/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef DEFERREDRENDERER_H
#define DEFERREDRENDERER_H

#include <Qt3DRender/QViewport>
#include <Qt3DRender/QClearBuffers>
#include <Qt3DRender/QLayerFilter>
#include <Qt3DRender/QRenderPassFilter>
#include <Qt3DRender/QRenderTargetSelector>
#include <Qt3DRender/QRenderSurfaceSelector>
#include <Qt3DRender/QCameraSelector>
#include <Qt3DRender/QFilterKey>

class GBuffer;

class DeferredRenderer : public Qt3DRender::QViewport
{
public:
    explicit DeferredRenderer(Qt3DCore::QNode *parent = 0);

    void setSceneCamera(Qt3DCore::QEntity *camera);
    void setGBuffer(Qt3DRender::QRenderTarget *gBuffer);
    void setGeometryPassCriteria(QList<Qt3DRender::QFilterKey *> criteria);
    void setFinalPassCriteria(QList<Qt3DRender::QFilterKey *> criteria);
    void setGBufferLayer(Qt3DRender::QLayer *layer);
    void setScreenQuadLayer(Qt3DRender::QLayer *layer);
    void setSurface(QWindow *surface);

private:
    Qt3DRender::QRenderSurfaceSelector *m_surfaceSelector;
    Qt3DRender::QLayerFilter *m_sceneFilter;
    Qt3DRender::QLayerFilter *m_screenQuadFilter;
    Qt3DRender::QClearBuffers *m_clearScreenQuad;
    Qt3DRender::QRenderTargetSelector *m_gBufferTargetSelector;
    Qt3DRender::QClearBuffers *m_clearGBuffer;
    Qt3DRender::QRenderPassFilter *m_geometryPassFilter;
    Qt3DRender::QRenderPassFilter *m_finalPassFilter;
    Qt3DRender::QCameraSelector *m_sceneCameraSelector;
    Qt3DRender::QParameter *m_winSize;
    GBuffer *m_gBuffer;
    QWindow *m_window;

    QMetaObject::Connection m_widthChangedConnection;
    QMetaObject::Connection m_heightChangedConnection;
};

#endif // DEFERREDRENDERER_H
