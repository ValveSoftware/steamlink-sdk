/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "declarativerendernode_p.h"
#include "abstractdeclarative_p.h"
#include <QtGui/QOpenGLFramebufferObject>
#include <QtCore/QMutexLocker>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

DeclarativeRenderNode::DeclarativeRenderNode(AbstractDeclarative *declarative,
                                             const QSharedPointer<QMutex> &nodeMutex)
    : QSGGeometryNode(),
      m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4),
      m_texture(0),
      m_declarative(declarative),
      m_controller(0),
      m_fbo(0),
      m_multisampledFBO(0),
      m_window(0),
      m_samples(0),
      m_dirtyFBO(false)
{
    m_nodeMutex = nodeMutex;
    setMaterial(&m_material);
    setOpaqueMaterial(&m_materialO);
    setGeometry(&m_geometry);
    setFlag(UsePreprocess);
}

DeclarativeRenderNode::~DeclarativeRenderNode()
{
    delete m_fbo;
    delete m_multisampledFBO;
    delete m_texture;

    m_nodeMutex.clear();
}

void DeclarativeRenderNode::setSize(const QSize &size)
{
    if (size == m_size)
        return;

    m_size = size;
    m_dirtyFBO = true;
    markDirty(DirtyGeometry);
}

void DeclarativeRenderNode::update()
{
    if (m_dirtyFBO) {
        updateFBO();
        m_dirtyFBO = false;
    }
}

void DeclarativeRenderNode::updateFBO()
{
    m_declarative->activateOpenGLContext(m_window);

    if (m_fbo)
        delete m_fbo;

    m_fbo = new QOpenGLFramebufferObject(m_size);
    m_fbo->setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);

    // Multisampled
    if (m_multisampledFBO) {
        delete m_multisampledFBO;
        m_multisampledFBO = 0;
    }
    if (m_samples > 0) {
        QOpenGLFramebufferObjectFormat multisampledFrambufferFormat;
        multisampledFrambufferFormat.setSamples(m_samples);
        multisampledFrambufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);

        m_multisampledFBO = new QOpenGLFramebufferObject(m_size, multisampledFrambufferFormat);
    }

    QSGGeometry::updateTexturedRectGeometry(&m_geometry,
                                            QRectF(0, 0,
                                                   m_size.width()
                                                   / m_controller->scene()->devicePixelRatio(),
                                                   m_size.height()
                                                   / m_controller->scene()->devicePixelRatio()),
                                            QRectF(0, 1, 1, -1));

    delete m_texture;
    m_texture = m_window->createTextureFromId(m_fbo->texture(), m_size);
    m_material.setTexture(m_texture);
    m_materialO.setTexture(m_texture);

    m_declarative->doneOpenGLContext(m_window);
}

void DeclarativeRenderNode::setQuickWindow(QQuickWindow *window)
{
    Q_ASSERT(window);

    m_window = window;
}

void DeclarativeRenderNode::setController(Abstract3DController *controller)
{
    QMutexLocker locker(m_nodeMutex.data());
    m_controller = controller;
    if (m_controller) {
        connect(m_controller, &QObject::destroyed,
                this, &DeclarativeRenderNode::handleControllerDestroyed, Qt::DirectConnection);
    }
}

void DeclarativeRenderNode::setSamples(int samples)
{
    if (m_samples == samples)
        return;

    m_samples = samples;
    m_dirtyFBO = true;
}

void DeclarativeRenderNode::preprocess()
{
    QMutexLocker locker(m_nodeMutex.data());

    if (!m_controller)
        return;

    QOpenGLFramebufferObject *targetFBO;
    if (m_samples > 0)
        targetFBO = m_multisampledFBO;
    else
        targetFBO = m_fbo;

    m_declarative->activateOpenGLContext(m_window);

    targetFBO->bind();
    // Render scene here
    m_controller->render(targetFBO->handle());

    targetFBO->release();

    if (m_samples > 0)
        QOpenGLFramebufferObject::blitFramebuffer(m_fbo, m_multisampledFBO);

    m_declarative->doneOpenGLContext(m_window);
}

// This function is called within m_nodeMutex lock
void DeclarativeRenderNode::handleControllerDestroyed()
{
    m_controller = 0;
}

QT_END_NAMESPACE_DATAVISUALIZATION
