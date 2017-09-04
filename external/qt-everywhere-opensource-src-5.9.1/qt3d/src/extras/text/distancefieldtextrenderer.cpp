/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qgeometry.h>
#include <Qt3DRender/qgeometryrenderer.h>

#include <Qt3DExtras/private/qtext2dmaterial_p.h>

#include "distancefieldtextrenderer_p.h"
#include "distancefieldtextrenderer_p_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DExtras {

using namespace Qt3DCore;

DistanceFieldTextRendererPrivate::DistanceFieldTextRendererPrivate()
    : m_renderer(nullptr)
    , m_geometry(nullptr)
    , m_positionAttr(nullptr)
    , m_texCoordAttr(nullptr)
    , m_indexAttr(nullptr)
    , m_vertexBuffer(nullptr)
    , m_indexBuffer(nullptr)
    , m_material(nullptr)
{
}

DistanceFieldTextRendererPrivate::~DistanceFieldTextRendererPrivate()
{
}

void DistanceFieldTextRendererPrivate::init()
{
    Q_Q(DistanceFieldTextRenderer);

    m_renderer = new Qt3DRender::QGeometryRenderer(q);
    m_renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);

    m_geometry = new Qt3DRender::QGeometry(m_renderer);
    m_renderer->setGeometry(m_geometry);

    m_vertexBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, m_geometry);
    m_indexBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::IndexBuffer, m_geometry);

    m_positionAttr = new Qt3DRender::QAttribute(m_geometry);
    m_positionAttr->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
    m_positionAttr->setVertexBaseType(Qt3DRender::QAttribute::Float);
    m_positionAttr->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
    m_positionAttr->setVertexSize(3);
    m_positionAttr->setByteStride(5 * sizeof(float));
    m_positionAttr->setByteOffset(0);
    m_positionAttr->setBuffer(m_vertexBuffer);

    m_texCoordAttr = new Qt3DRender::QAttribute(m_geometry);
    m_texCoordAttr->setName(Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName());
    m_texCoordAttr->setVertexBaseType(Qt3DRender::QAttribute::Float);
    m_texCoordAttr->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
    m_texCoordAttr->setVertexSize(2);
    m_texCoordAttr->setByteStride(5 * sizeof(float));
    m_texCoordAttr->setByteOffset(3 * sizeof(float));
    m_texCoordAttr->setBuffer(m_vertexBuffer);

    m_indexAttr = new Qt3DRender::QAttribute(m_geometry);
    m_indexAttr->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
    m_indexAttr->setVertexBaseType(Qt3DRender::QAttribute::UnsignedShort);
    m_indexAttr->setBuffer(m_indexBuffer);

    m_geometry->addAttribute(m_positionAttr);
    m_geometry->setBoundingVolumePositionAttribute(m_positionAttr);
    m_geometry->addAttribute(m_texCoordAttr);
    m_geometry->addAttribute(m_indexAttr);

    m_material = new QText2DMaterial(q);

    q->addComponent(m_renderer);
    q->addComponent(m_material);
}

DistanceFieldTextRenderer::DistanceFieldTextRenderer(QNode *parent)
    : QEntity(*new DistanceFieldTextRendererPrivate(), parent)
{
    Q_D(DistanceFieldTextRenderer);
    d->init();
}

DistanceFieldTextRenderer::~DistanceFieldTextRenderer()
{
}

void DistanceFieldTextRenderer::setGlyphData(Qt3DRender::QAbstractTexture *glyphTexture,
                                             const QVector<float> &vertexData,
                                             const QVector<quint16> &indexData)
{
    Q_D(DistanceFieldTextRenderer);

    const int vertexCount = vertexData.size() / 5;

    d->m_vertexBuffer->setData(QByteArray((char*) vertexData.data(), vertexData.size() * sizeof(float)));
    d->m_indexBuffer->setData(QByteArray((char*) indexData.data(), indexData.size() * sizeof(quint16)));
    d->m_positionAttr->setCount(vertexCount);
    d->m_texCoordAttr->setCount(vertexCount);
    d->m_indexAttr->setCount(indexData.size());

    d->m_material->setDistanceFieldTexture(glyphTexture);
}

void DistanceFieldTextRenderer::setColor(const QColor &color)
{
    Q_D(DistanceFieldTextRenderer);
    d->m_material->setColor(color);
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
