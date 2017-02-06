/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qsgd3d12renderer_p.h"
#include "qsgd3d12rendercontext_p.h"
#include <private/qsgnodeupdater_p.h>
#include <private/qsgrendernode_p.h>

#include "vs_stencilclip.hlslh"
#include "ps_stencilclip.hlslh"

//#define I_LIKE_STENCIL

QT_BEGIN_NAMESPACE

#define QSGNODE_TRAVERSE(NODE) for (QSGNode *child = NODE->firstChild(); child; child = child->nextSibling())

// NOTE: Avoid categorized logging. It is slow.

#define DECLARE_DEBUG_VAR(variable) \
    static bool debug_ ## variable() \
    { static bool value = qgetenv("QSG_RENDERER_DEBUG").contains(QT_STRINGIFY(variable)); return value; }

DECLARE_DEBUG_VAR(build)
DECLARE_DEBUG_VAR(change)
DECLARE_DEBUG_VAR(render)

class DummyUpdater : public QSGNodeUpdater
{
public:
    void updateState(QSGNode *) { };
};

QSGD3D12Renderer::QSGD3D12Renderer(QSGRenderContext *context)
    : QSGRenderer(context),
      m_renderList(16),
      m_vboData(1024),
      m_iboData(256),
      m_cboData(4096)
{
    setNodeUpdater(new DummyUpdater);
}

QSGD3D12Renderer::~QSGD3D12Renderer()
{
    if (m_engine) {
        m_engine->releaseBuffer(m_vertexBuf);
        m_engine->releaseBuffer(m_indexBuf);
        m_engine->releaseBuffer(m_constantBuf);
    }
}

void QSGD3D12Renderer::renderScene(GLuint fboId)
{
    m_renderTarget = fboId;

    struct DummyBindable : public QSGBindable {
        void bind() const { }
    } bindable;

    QSGRenderer::renderScene(bindable); // calls back render()
}

// Search through the node set and remove nodes that are descendants of other
// nodes in the same set.
static QSet<QSGNode *> qsg_removeDescendants(const QSet<QSGNode *> &nodes, QSGRootNode *root)
{
    QSet<QSGNode *> result = nodes;
    for (QSGNode *node : nodes) {
        QSGNode *n = node;
        while (n != root) {
            if (n != node && result.contains(n)) {
                result.remove(node);
                break;
            }
            n = n->parent();
        }
    }
    return result;
}

void QSGD3D12Renderer::updateMatrices(QSGNode *node, QSGTransformNode *xform)
{
    if (node->isSubtreeBlocked())
        return;

    if (node->type() == QSGNode::TransformNodeType) {
        QSGTransformNode *tn = static_cast<QSGTransformNode *>(node);
        if (xform)
            tn->setCombinedMatrix(xform->combinedMatrix() * tn->matrix());
        else
            tn->setCombinedMatrix(tn->matrix());
        QSGNODE_TRAVERSE(node)
            updateMatrices(child, tn);
    } else {
        if (node->type() == QSGNode::GeometryNodeType || node->type() == QSGNode::ClipNodeType) {
            m_nodeDirtyMap[node] |= QSGD3D12MaterialRenderState::DirtyMatrix;
            QSGBasicGeometryNode *gnode = static_cast<QSGBasicGeometryNode *>(node);
            const QMatrix4x4 *newMatrix = xform ? &xform->combinedMatrix() : nullptr;
            // NB the newMatrix ptr is usually the same as before as it just
            // references the transform node's own matrix.
            gnode->setRendererMatrix(newMatrix);
        }
        QSGNODE_TRAVERSE(node)
            updateMatrices(child, xform);
    }
}

void QSGD3D12Renderer::updateOpacities(QSGNode *node, float inheritedOpacity)
{
    if (node->isSubtreeBlocked())
        return;

    if (node->type() == QSGNode::OpacityNodeType) {
        QSGOpacityNode *on = static_cast<QSGOpacityNode *>(node);
        float combined = inheritedOpacity * on->opacity();
        on->setCombinedOpacity(combined);
        QSGNODE_TRAVERSE(node)
            updateOpacities(child, combined);
    } else {
        if (node->type() == QSGNode::GeometryNodeType) {
            m_nodeDirtyMap[node] |= QSGD3D12MaterialRenderState::DirtyOpacity;
            QSGGeometryNode *gn = static_cast<QSGGeometryNode *>(node);
            gn->setInheritedOpacity(inheritedOpacity);
        }
        QSGNODE_TRAVERSE(node)
            updateOpacities(child, inheritedOpacity);
    }
}

void QSGD3D12Renderer::buildRenderList(QSGNode *node, QSGClipNode *clip)
{
    if (node->isSubtreeBlocked())
        return;

    if (node->type() == QSGNode::GeometryNodeType || node->type() == QSGNode::ClipNodeType) {
        QSGBasicGeometryNode *gn = static_cast<QSGBasicGeometryNode *>(node);
        QSGGeometry *g = gn->geometry();

        Element e;
        e.node = gn;

        if (g->vertexCount() > 0) {
            e.vboOffset = m_vboData.size();
            const int vertexSize = g->sizeOfVertex() * g->vertexCount();
            m_vboData.resize(m_vboData.size() + vertexSize);
            memcpy(m_vboData.data() + e.vboOffset, g->vertexData(), vertexSize);
        }

        if (g->indexCount() > 0) {
            e.iboOffset = m_iboData.size();
            e.iboStride = g->sizeOfIndex();
            const int indexSize = e.iboStride * g->indexCount();
            m_iboData.resize(m_iboData.size() + indexSize);
            memcpy(m_iboData.data() + e.iboOffset, g->indexData(), indexSize);
        }

        e.cboOffset = m_cboData.size();
        if (node->type() == QSGNode::GeometryNodeType) {
            QSGD3D12Material *m = static_cast<QSGD3D12Material *>(static_cast<QSGGeometryNode *>(node)->activeMaterial());
            e.cboSize = m->constantBufferSize();
        } else {
            // Stencil-based clipping needs a 4x4 matrix.
            e.cboSize = QSGD3D12Engine::alignedConstantBufferSize(16 * sizeof(float));
        }
        m_cboData.resize(m_cboData.size() + e.cboSize);

        m_renderList.add(e);

        gn->setRendererClipList(clip);
        if (node->type() == QSGNode::ClipNodeType)
            clip = static_cast<QSGClipNode *>(node);
    } else if (node->type() == QSGNode::RenderNodeType) {
        QSGRenderNode *rn = static_cast<QSGRenderNode *>(node);
        Element e;
        e.node = rn;
        m_renderList.add(e);
    }

    QSGNODE_TRAVERSE(node)
        buildRenderList(child, clip);
}

void QSGD3D12Renderer::render()
{
    QSGD3D12RenderContext *rc = static_cast<QSGD3D12RenderContext *>(context());
    m_engine = rc->engine();
    if (!m_layerRenderer)
        m_engine->beginFrame();
    else
        m_engine->beginLayer();

    m_activeScissorRect = QRect();

    if (m_rebuild) {
        m_rebuild = false;

        m_dirtyTransformNodes.clear();
        m_dirtyTransformNodes.insert(rootNode());
        m_dirtyOpacityNodes.clear();
        m_dirtyOpacityNodes.insert(rootNode());

        m_renderList.reset();
        m_vboData.reset();
        m_iboData.reset();
        m_cboData.reset();

        buildRenderList(rootNode(), nullptr);

        if (!m_vertexBuf)
            m_vertexBuf = m_engine->genBuffer();
        m_engine->resetBuffer(m_vertexBuf, m_vboData.data(), m_vboData.size());

        if (!m_constantBuf)
            m_constantBuf = m_engine->genBuffer();
        m_engine->resetBuffer(m_constantBuf, m_cboData.data(), m_cboData.size());

        if (m_iboData.size()) {
            if (!m_indexBuf)
                m_indexBuf = m_engine->genBuffer();
            m_engine->resetBuffer(m_indexBuf, m_iboData.data(), m_iboData.size());
        } else if (m_indexBuf) {
            m_engine->releaseBuffer(m_indexBuf);
            m_indexBuf = 0;
        }

        if (Q_UNLIKELY(debug_build())) {
            qDebug("renderList: %d elements in total", m_renderList.size());
            for (int i = 0; i < m_renderList.size(); ++i) {
                const Element &e = m_renderList.at(i);
                qDebug() << " - " << e.vboOffset << e.iboOffset << e.cboOffset << e.cboSize << e.node;
            }
        }
    }

    const QRect devRect = deviceRect();
    m_projectionChangedDueToDeviceSize = devRect != m_lastDeviceRect;
    if (m_projectionChangedDueToDeviceSize)
        m_lastDeviceRect = devRect;

    if (m_dirtyTransformNodes.size()) {
        const QSet<QSGNode *> subTreeRoots = qsg_removeDescendants(m_dirtyTransformNodes, rootNode());
        for (QSGNode *node : subTreeRoots) {
            // First find the parent transform so we have the accumulated
            // matrix up until this point.
            QSGTransformNode *xform = 0;
            QSGNode *n = node;
            if (n->type() == QSGNode::TransformNodeType)
                n = node->parent();
            while (n != rootNode() && n->type() != QSGNode::TransformNodeType)
                n = n->parent();
            if (n != rootNode())
                xform = static_cast<QSGTransformNode *>(n);

            // Then update in the subtree
            updateMatrices(node, xform);
        }
    }

    if (m_dirtyOpacityNodes.size()) {
        const QSet<QSGNode *> subTreeRoots = qsg_removeDescendants(m_dirtyOpacityNodes, rootNode());
        for (QSGNode *node : subTreeRoots) {
            float opacity = 1.0f;
            QSGNode *n = node;
            if (n->type() == QSGNode::OpacityNodeType)
                n = node->parent();
            while (n != rootNode() && n->type() != QSGNode::OpacityNodeType)
                n = n->parent();
            if (n != rootNode())
                opacity = static_cast<QSGOpacityNode *>(n)->combinedOpacity();

            updateOpacities(node, opacity);
        }
        m_dirtyOpaqueElements = true;
    }

    if (m_dirtyOpaqueElements) {
        m_dirtyOpaqueElements = false;
        m_opaqueElements.clear();
        m_opaqueElements.resize(m_renderList.size());
        for (int i = 0; i < m_renderList.size(); ++i) {
            const Element &e = m_renderList.at(i);
            if (e.node->type() == QSGNode::GeometryNodeType) {
                const QSGGeometryNode *gn = static_cast<QSGGeometryNode *>(e.node);
                if (gn->inheritedOpacity() > 0.999f && ((gn->activeMaterial()->flags() & QSGMaterial::Blending) == 0))
                    m_opaqueElements.setBit(i);
            }
            // QSGRenderNodes are always treated as non-opaque
        }
    }

    // Build pipeline state and draw calls.
    renderElements();

    m_dirtyTransformNodes.clear();
    m_dirtyOpacityNodes.clear();
    m_dirtyOpaqueElements = false;
    m_nodeDirtyMap.clear();

    // Finalize buffers and execute commands.
    if (!m_layerRenderer)
        m_engine->endFrame();
    else
        m_engine->endLayer();
}

void QSGD3D12Renderer::nodeChanged(QSGNode *node, QSGNode::DirtyState state)
{
    // note that with DirtyNodeRemoved the window and all the graphics engine may already be gone

    if (Q_UNLIKELY(debug_change())) {
        QDebug debug = qDebug();
        debug << "dirty:";
        if (state & QSGNode::DirtyGeometry)
            debug << "Geometry";
        if (state & QSGNode::DirtyMaterial)
            debug << "Material";
        if (state & QSGNode::DirtyMatrix)
            debug << "Matrix";
        if (state & QSGNode::DirtyNodeAdded)
            debug << "Added";
        if (state & QSGNode::DirtyNodeRemoved)
            debug << "Removed";
        if (state & QSGNode::DirtyOpacity)
            debug << "Opacity";
        if (state & QSGNode::DirtySubtreeBlocked)
            debug << "SubtreeBlocked";
        if (state & QSGNode::DirtyForceUpdate)
            debug << "ForceUpdate";

        // when removed, some parts of the node could already have been destroyed
        // so don't debug it out.
        if (state & QSGNode::DirtyNodeRemoved)
            debug << (void *) node << node->type();
        else
            debug << node;
    }

    if (state & (QSGNode::DirtyNodeAdded
                 | QSGNode::DirtyNodeRemoved
                 | QSGNode::DirtySubtreeBlocked
                 | QSGNode::DirtyGeometry
                 | QSGNode::DirtyForceUpdate))
        m_rebuild = true;

    if (state & QSGNode::DirtyMatrix)
        m_dirtyTransformNodes << node;

    if (state & QSGNode::DirtyOpacity)
        m_dirtyOpacityNodes << node;

    if (state & QSGNode::DirtyMaterial)
        m_dirtyOpaqueElements = true;

    QSGRenderer::nodeChanged(node, state);
}

void QSGD3D12Renderer::renderElements()
{
    m_engine->queueSetRenderTarget(m_renderTarget);
    m_engine->queueViewport(viewportRect());
    m_engine->queueClearRenderTarget(clearColor());
    m_engine->queueClearDepthStencil(1, 0, QSGD3D12Engine::ClearDepth | QSGD3D12Engine::ClearStencil);

    m_pipelineState.blend = m_freshPipelineState.blend = QSGD3D12PipelineState::BlendNone;
    m_pipelineState.depthEnable = m_freshPipelineState.depthEnable = true;
    m_pipelineState.depthWrite = m_freshPipelineState.depthWrite = true;

    // First do opaque...
    // The algorithm is quite simple. We traverse the list back-to-front, and
    // for every item we start a second traversal and draw all elements which
    // have identical material. Then we clear the bit for this in the rendered
    // list so we don't draw it again when we come to that index.
    QBitArray rendered = m_opaqueElements;
    for (int i = m_renderList.size() - 1; i >= 0; --i) {
        if (rendered.testBit(i)) {
            renderElement(i);
            for (int j = i - 1; j >= 0; --j) {
                if (rendered.testBit(j)) {
                    const QSGGeometryNode *gni = static_cast<QSGGeometryNode *>(m_renderList.at(i).node);
                    const QSGGeometryNode *gnj = static_cast<QSGGeometryNode *>(m_renderList.at(j).node);
                    if (gni->clipList() == gnj->clipList()
                            && gni->inheritedOpacity() == gnj->inheritedOpacity()
                            && gni->geometry()->drawingMode() == gnj->geometry()->drawingMode()
                            && gni->geometry()->attributes() == gnj->geometry()->attributes()) {
                        const QSGMaterial *ami = gni->activeMaterial();
                        const QSGMaterial *amj = gnj->activeMaterial();
                        if (ami->type() == amj->type()
                                && ami->flags() == amj->flags()
                                && ami->compare(amj) == 0) {
                            renderElement(j);
                            rendered.clearBit(j);
                        }
                    }
                }
            }
        }
    }

    m_pipelineState.blend = m_freshPipelineState.blend = QSGD3D12PipelineState::BlendPremul;
    m_pipelineState.depthWrite = m_freshPipelineState.depthWrite = false;

    // ...then the alpha ones
    for (int i = 0; i < m_renderList.size(); ++i) {
        if ((m_renderList.at(i).node->type() == QSGNode::GeometryNodeType && !m_opaqueElements.testBit(i))
                || m_renderList.at(i).node->type() == QSGNode::RenderNodeType)
            renderElement(i);
    }
}

struct RenderNodeState : public QSGRenderNode::RenderState
{
    const QMatrix4x4 *projectionMatrix() const override { return m_projectionMatrix; }
    QRect scissorRect() const { return m_scissorRect; }
    bool scissorEnabled() const { return m_scissorEnabled; }
    int stencilValue() const { return m_stencilValue; }
    bool stencilEnabled() const { return m_stencilEnabled; }
    const QRegion *clipRegion() const override { return nullptr; }

    const QMatrix4x4 *m_projectionMatrix;
    QRect m_scissorRect;
    bool m_scissorEnabled;
    int m_stencilValue;
    bool m_stencilEnabled;
};

void QSGD3D12Renderer::renderElement(int elementIndex)
{
    Element &e = m_renderList.at(elementIndex);
    Q_ASSERT(e.node->type() == QSGNode::GeometryNodeType || e.node->type() == QSGNode::RenderNodeType);

    if (e.node->type() == QSGNode::RenderNodeType) {
        renderRenderNode(static_cast<QSGRenderNode *>(e.node), elementIndex);
        return;
    }

    if (e.vboOffset < 0)
        return;

    Q_ASSERT(e.cboOffset >= 0);

    const QSGGeometryNode *gn = static_cast<QSGGeometryNode *>(e.node);
    if (Q_UNLIKELY(debug_render()))
        qDebug() << "renderElement:" << elementIndex << gn << e.vboOffset << e.iboOffset << gn->inheritedOpacity() << gn->clipList();

    if (gn->inheritedOpacity() < 0.001f) // pretty much invisible, don't draw it
        return;

    // Update the QSGRenderer members which the materials will access.
    m_current_projection_matrix = projectionMatrix();
    const float scale = 1.0 / m_renderList.size();
    m_current_projection_matrix(2, 2) = scale;
    m_current_projection_matrix(2, 3) = 1.0f - (elementIndex + 1) * scale;
    m_current_model_view_matrix = gn->matrix() ? *gn->matrix() : QMatrix4x4();
    m_current_determinant = m_current_model_view_matrix.determinant();
    m_current_opacity = gn->inheritedOpacity();

    const QSGGeometry *g = gn->geometry();
    QSGD3D12Material *m = static_cast<QSGD3D12Material *>(gn->activeMaterial());

    if (m->type() != m_lastMaterialType) {
        m_pipelineState = m_freshPipelineState;
        m->preparePipeline(&m_pipelineState);
    }

    QSGD3D12MaterialRenderState::DirtyStates dirtyState = m_nodeDirtyMap.value(e.node);

    // After a rebuild everything in the cbuffer has to be updated.
    if (!e.cboPrepared) {
        e.cboPrepared = true;
        dirtyState = QSGD3D12MaterialRenderState::DirtyAll;
    }

    // DirtyMatrix does not include projection matrix changes that can arise
    // due to changing the render target's size (and there is no rebuild).
    // Accommodate for this.
    if (m_projectionChangedDueToDeviceSize)
        dirtyState |= QSGD3D12MaterialRenderState::DirtyMatrix;

    quint8 *cboPtr = nullptr;
    if (e.cboSize > 0)
        cboPtr = m_cboData.data() + e.cboOffset;

    if (Q_UNLIKELY(debug_render()))
        qDebug() << "dirty state for" << e.node << "is" << dirtyState;

    QSGD3D12Material::ExtraState extraState;
    QSGD3D12Material::UpdateResults updRes = m->updatePipeline(state(dirtyState),
                                                               &m_pipelineState,
                                                               &extraState,
                                                               cboPtr);

    if (updRes.testFlag(QSGD3D12Material::UpdatedConstantBuffer))
        m_engine->markBufferDirty(m_constantBuf, e.cboOffset, e.cboSize);

    if (updRes.testFlag(QSGD3D12Material::UpdatedBlendFactor))
        m_engine->queueSetBlendFactor(extraState.blendFactor);

    setInputLayout(g, &m_pipelineState);

    m_lastMaterialType = m->type();

    setupClipping(gn->clipList(), elementIndex);

    // ### Lines and points with sizes other than 1 have to be implemented in some other way. Just ignore for now.
    if (g->drawingMode() == QSGGeometry::DrawLineStrip || g->drawingMode() == QSGGeometry::DrawLines) {
        if (g->lineWidth() != 1.0f)
            qWarning("QSGD3D12Renderer: Line widths other than 1 are not supported by this renderer");
    } else if (g->drawingMode() == QSGGeometry::DrawPoints) {
        if (g->lineWidth() != 1.0f)
            qWarning("QSGD3D12Renderer: Point sprites are not supported by this renderer");
    }

    m_engine->finalizePipeline(m_pipelineState);

    queueDrawCall(g, e);
}

void QSGD3D12Renderer::setInputLayout(const QSGGeometry *g, QSGD3D12PipelineState *pipelineState)
{
    pipelineState->inputElementCount = g->attributeCount();
    const QSGGeometry::Attribute *attrs = g->attributes();
    quint32 offset = 0;
    for (int i = 0; i < g->attributeCount(); ++i) {
        QSGD3D12InputElement &ie(pipelineState->inputElements[i]);
        static const char *semanticNames[] = { "UNKNOWN", "POSITION", "COLOR", "TEXCOORD", "TEXCOORD", "TEXCOORD" };
        static const int semanticIndices[] = { 0, 0, 0, 0, 1, 2 };
        const int semantic = attrs[i].attributeType;
        Q_ASSERT(semantic >= 1 && semantic < _countof(semanticNames));
        const int tupleSize = attrs[i].tupleSize;
        ie.semanticName = semanticNames[semantic];
        ie.semanticIndex = semanticIndices[semantic];
        ie.offset = offset;
        int bytesPerTuple = 0;
        ie.format = QSGD3D12Engine::toDXGIFormat(QSGGeometry::Type(attrs[i].type), tupleSize, &bytesPerTuple);
        if (ie.format == FmtUnknown)
            qFatal("QSGD3D12Renderer: unsupported tuple size for attribute type 0x%x", attrs[i].type);
        offset += bytesPerTuple;
        // There is one buffer with interleaved data so the slot is always 0.
        ie.slot = 0;
    }
}

void QSGD3D12Renderer::queueDrawCall(const QSGGeometry *g, const QSGD3D12Renderer::Element &e)
{
    QSGD3D12Engine::DrawParams dp;
    dp.mode = QSGGeometry::DrawingMode(g->drawingMode());
    dp.vertexBuf = m_vertexBuf;
    dp.constantBuf = m_constantBuf;
    dp.vboOffset = e.vboOffset;
    dp.vboSize = g->vertexCount() * g->sizeOfVertex();
    dp.vboStride = g->sizeOfVertex();
    dp.cboOffset = e.cboOffset;

    if (e.iboOffset >= 0) {
        const QSGGeometry::Type indexType = QSGGeometry::Type(g->indexType());
        const QSGD3D12Format indexFormat = QSGD3D12Engine::toDXGIFormat(indexType);
        if (indexFormat == FmtUnknown)
            qFatal("QSGD3D12Renderer: unsupported index type 0x%x", indexType);
        dp.count = g->indexCount();
        dp.indexBuf = m_indexBuf;
        dp.startIndexIndex = e.iboOffset / e.iboStride;
        dp.indexFormat = indexFormat;
    } else {
        dp.count = g->vertexCount();
    }

    m_engine->queueDraw(dp);
}

void QSGD3D12Renderer::setupClipping(const QSGClipNode *clip, int elementIndex)
{
    const QRect devRect = deviceRect();
    QRect scissorRect;
    int clipTypes = 0;
    quint32 stencilValue = 0;

    while (clip) {
        QMatrix4x4 m = projectionMatrix();
        if (clip->matrix())
            m *= *clip->matrix();

#ifndef I_LIKE_STENCIL
        const bool isRectangleWithNoPerspective = clip->isRectangular()
                && qFuzzyIsNull(m(3, 0)) && qFuzzyIsNull(m(3, 1));
        const bool noRotate = qFuzzyIsNull(m(0, 1)) && qFuzzyIsNull(m(1, 0));
        const bool isRotate90 = qFuzzyIsNull(m(0, 0)) && qFuzzyIsNull(m(1, 1));

        if (isRectangleWithNoPerspective && (noRotate || isRotate90)) {
            QRectF bbox = clip->clipRect();
            float invW = 1.0f / m(3, 3);
            float fx1, fy1, fx2, fy2;
            if (noRotate) {
                fx1 = (bbox.left() * m(0, 0) + m(0, 3)) * invW;
                fy1 = (bbox.bottom() * m(1, 1) + m(1, 3)) * invW;
                fx2 = (bbox.right() * m(0, 0) + m(0, 3)) * invW;
                fy2 = (bbox.top() * m(1, 1) + m(1, 3)) * invW;
            } else {
                Q_ASSERT(isRotate90);
                fx1 = (bbox.bottom() * m(0, 1) + m(0, 3)) * invW;
                fy1 = (bbox.left() * m(1, 0) + m(1, 3)) * invW;
                fx2 = (bbox.top() * m(0, 1) + m(0, 3)) * invW;
                fy2 = (bbox.right() * m(1, 0) + m(1, 3)) * invW;
            }

            if (fx1 > fx2)
                qSwap(fx1, fx2);
            if (fy1 > fy2)
                qSwap(fy1, fy2);

            int ix1 = qRound((fx1 + 1) * devRect.width() * 0.5f);
            int iy1 = qRound((fy1 + 1) * devRect.height() * 0.5f);
            int ix2 = qRound((fx2 + 1) * devRect.width() * 0.5f);
            int iy2 = qRound((fy2 + 1) * devRect.height() * 0.5f);

            if (!(clipTypes & ClipScissor)) {
                scissorRect = QRect(ix1, devRect.height() - iy2, ix2 - ix1, iy2 - iy1);
                clipTypes |= ClipScissor;
            } else {
                scissorRect &= QRect(ix1, devRect.height() - iy2, ix2 - ix1, iy2 - iy1);
            }
        } else
#endif
        {
            clipTypes |= ClipStencil;
            renderStencilClip(clip, elementIndex, m, stencilValue);
        }

        clip = clip->clipList();
    }

    setScissor((clipTypes & ClipScissor) ? scissorRect : viewportRect());

    if (clipTypes & ClipStencil) {
        m_pipelineState.stencilEnable = true;
        m_engine->queueSetStencilRef(stencilValue);
        m_currentStencilValue = stencilValue;
    } else {
        m_pipelineState.stencilEnable = false;
        m_currentStencilValue = 0;
    }

    m_currentClipTypes = clipTypes;
}

void QSGD3D12Renderer::setScissor(const QRect &r)
{
    if (m_activeScissorRect == r)
        return;

    m_activeScissorRect = r;
    m_engine->queueScissor(r);
}

void QSGD3D12Renderer::renderStencilClip(const QSGClipNode *clip, int elementIndex,
                                         const QMatrix4x4 &m, quint32 &stencilValue)
{
    QSGD3D12PipelineState sps;
    sps.shaders.vs = g_VS_StencilClip;
    sps.shaders.vsSize = sizeof(g_VS_StencilClip);
    sps.shaders.ps = g_PS_StencilClip;
    sps.shaders.psSize = sizeof(g_PS_StencilClip);

    m_engine->queueClearDepthStencil(1, 0, QSGD3D12Engine::ClearStencil);
    sps.stencilEnable = true;
    sps.colorWrite = false;
    sps.depthWrite = false;

    sps.stencilFunc = QSGD3D12PipelineState::CompareEqual;
    sps.stencilFailOp = QSGD3D12PipelineState::StencilKeep;
    sps.stencilDepthFailOp = QSGD3D12PipelineState::StencilKeep;
    sps.stencilPassOp = QSGD3D12PipelineState::StencilIncr;

    m_engine->queueSetStencilRef(stencilValue);

    int clipIndex = elementIndex;
    while (m_renderList.at(--clipIndex).node != clip) {
        Q_ASSERT(clipIndex >= 0);
    }
    const Element &ce = m_renderList.at(clipIndex);
    Q_ASSERT(ce.node == clip);

    const QSGGeometry *g = clip->geometry();
    Q_ASSERT(g->attributeCount() == 1);
    Q_ASSERT(g->attributes()[0].tupleSize == 2);
    Q_ASSERT(g->attributes()[0].type == QSGGeometry::FloatType);

    setInputLayout(g, &sps);
    m_engine->finalizePipeline(sps);

    Q_ASSERT(ce.cboSize > 0);
    quint8 *p = m_cboData.data() + ce.cboOffset;
    memcpy(p, m.constData(), 16 * sizeof(float));
    m_engine->markBufferDirty(m_constantBuf, ce.cboOffset, ce.cboSize);

    queueDrawCall(g, ce);

    ++stencilValue;
}

void QSGD3D12Renderer::renderRenderNode(QSGRenderNode *node, int elementIndex)
{
    QSGRenderNodePrivate *rd = QSGRenderNodePrivate::get(node);
    RenderNodeState state;

    setupClipping(rd->m_clip_list, elementIndex);

    QMatrix4x4 pm = projectionMatrix();
    state.m_projectionMatrix = &pm;
    state.m_scissorEnabled = m_currentClipTypes & ClipScissor;
    state.m_stencilEnabled = m_currentClipTypes & ClipStencil;
    state.m_scissorRect = m_activeScissorRect;
    state.m_stencilValue = m_currentStencilValue;

    // ### rendernodes do not have the QSGBasicGeometryNode infrastructure
    // for storing combined matrices, opacity and such, but perhaps they should.
    QSGNode *xform = node->parent();
    QSGNode *root = rootNode();
    QMatrix4x4 modelview;
    while (xform != root) {
        if (xform->type() == QSGNode::TransformNodeType) {
            modelview *= static_cast<QSGTransformNode *>(xform)->combinedMatrix();
            break;
        }
        xform = xform->parent();
    }
    rd->m_matrix = &modelview;

    QSGNode *opacity = node->parent();
    rd->m_opacity = 1.0;
    while (opacity != rootNode()) {
        if (opacity->type() == QSGNode::OpacityNodeType) {
            rd->m_opacity = static_cast<QSGOpacityNode *>(opacity)->combinedOpacity();
            break;
        }
        opacity = opacity->parent();
    }

    node->render(&state);

    m_engine->invalidateCachedFrameState();
    // For simplicity, reset viewport, scissor, blend factor, stencil ref when
    // any of them got changed. This will likely be rare so skip these otherwise.
    // Render target, pipeline state, draw call related stuff will be reset always.
    const bool restoreMinimal = node->changedStates() == 0;
    m_engine->restoreFrameState(restoreMinimal);
}

QT_END_NAMESPACE
