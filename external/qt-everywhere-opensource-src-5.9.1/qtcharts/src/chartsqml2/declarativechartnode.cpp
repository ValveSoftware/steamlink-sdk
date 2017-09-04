/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
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

#include "declarativechartnode.h"
#include "declarativeabstractrendernode.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGImageNode>
#include <QtQuick/QSGRendererInterface>

#ifndef QT_NO_OPENGL
# include "declarativeopenglrendernode.h"
#endif

QT_CHARTS_BEGIN_NAMESPACE

// This node handles displaying of the chart itself
DeclarativeChartNode::DeclarativeChartNode(QQuickWindow *window) :
    QSGRootNode(),
    m_window(window),
    m_renderNode(nullptr),
    m_imageNode(nullptr)
{
    // Create a DeclarativeRenderNode for correct QtQuick Backend
#ifndef QT_NO_OPENGL
    if (m_window->rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL)
        m_renderNode = new DeclarativeOpenGLRenderNode(m_window);
#endif

    if (m_renderNode) {
        m_renderNode->setFlag(OwnedByParent);
        appendChildNode(m_renderNode);
        m_renderNode->setRect(QRectF(0, 0, 0, 0)); // Hide child node by default
    }
}

DeclarativeChartNode::~DeclarativeChartNode()
{
}

// Must be called on render thread and in context
void DeclarativeChartNode::createTextureFromImage(const QImage &chartImage)
{
    static auto const defaultTextureOptions = QQuickWindow::CreateTextureOptions(QQuickWindow::TextureHasAlphaChannel |
                                                                                 QQuickWindow::TextureOwnsGLTexture);

    auto texture = m_window->createTextureFromImage(chartImage, defaultTextureOptions);
    // Create Image node if needed
    if (!m_imageNode) {
        m_imageNode = m_window->createImageNode();
        m_imageNode->setFlag(OwnedByParent);
        m_imageNode->setOwnsTexture(true);
        m_imageNode->setTexture(texture);
        prependChildNode(m_imageNode);
    } else {
        m_imageNode->setTexture(texture);
    }
    if (!m_rect.isEmpty())
        m_imageNode->setRect(m_rect);
}

void DeclarativeChartNode::setRect(const QRectF &rect)
{
    m_rect = rect;

    if (m_imageNode)
        m_imageNode->setRect(rect);
}

QT_CHARTS_END_NAMESPACE
