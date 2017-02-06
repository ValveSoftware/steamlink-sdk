/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Copyright (C) 2016 Robin Burchell <robin.burchell@viroteck.net>
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

#include "qsgbatchrenderer_p.h"
#include <private/qsgshadersourcebuilder_p.h>

#include <QQuickWindow>

#include <qmath.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QtNumeric>

#include <QtGui/QGuiApplication>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QtGui/QOpenGLFunctions_1_0>
#include <QtGui/QOpenGLFunctions_3_2_Core>

#include <private/qquickprofiler_p.h>
#include "qsgmaterialshader_p.h"

#include <algorithm>

#ifndef GL_DOUBLE
   #define GL_DOUBLE 0x140A
#endif

QT_BEGIN_NAMESPACE

extern QByteArray qsgShaderRewriter_insertZAttributes(const char *input, QSurfaceFormat::OpenGLContextProfile profile);

int qt_sg_envInt(const char *name, int defaultValue);

namespace QSGBatchRenderer
{

#define DECLARE_DEBUG_VAR(variable) \
    static bool debug_ ## variable() \
    { static bool value = qgetenv("QSG_RENDERER_DEBUG").contains(QT_STRINGIFY(variable)); return value; }
DECLARE_DEBUG_VAR(render)
DECLARE_DEBUG_VAR(build)
DECLARE_DEBUG_VAR(change)
DECLARE_DEBUG_VAR(upload)
DECLARE_DEBUG_VAR(roots)
DECLARE_DEBUG_VAR(dump)
DECLARE_DEBUG_VAR(noalpha)
DECLARE_DEBUG_VAR(noopaque)
DECLARE_DEBUG_VAR(noclip)
#undef DECLARE_DEBUG_VAR

static QElapsedTimer qsg_renderer_timer;

#define QSGNODE_TRAVERSE(NODE) for (QSGNode *child = NODE->firstChild(); child; child = child->nextSibling())
#define SHADOWNODE_TRAVERSE(NODE) for (Node *child = NODE->firstChild(); child; child = child->sibling())

static inline int size_of_type(GLenum type)
{
    static int sizes[] = {
        sizeof(char),
        sizeof(unsigned char),
        sizeof(short),
        sizeof(unsigned short),
        sizeof(int),
        sizeof(unsigned int),
        sizeof(float),
        2,
        3,
        4,
        sizeof(double)
    };
    Q_ASSERT(type >= GL_BYTE && type <= 0x140A); // the value of GL_DOUBLE
    return sizes[type - GL_BYTE];
}

bool qsg_sort_element_increasing_order(Element *a, Element *b) { return a->order < b->order; }
bool qsg_sort_element_decreasing_order(Element *a, Element *b) { return a->order > b->order; }
bool qsg_sort_batch_is_valid(Batch *a, Batch *b) { return a->first && !b->first; }
bool qsg_sort_batch_increasing_order(Batch *a, Batch *b) { return a->first->order < b->first->order; }
bool qsg_sort_batch_decreasing_order(Batch *a, Batch *b) { return a->first->order > b->first->order; }

QSGMaterial::Flag QSGMaterial_FullMatrix = (QSGMaterial::Flag) (QSGMaterial::RequiresFullMatrix & ~QSGMaterial::RequiresFullMatrixExceptTranslate);

struct QMatrix4x4_Accessor
{
    float m[4][4];
    int flagBits;

    static bool isTranslate(const QMatrix4x4 &m) { return ((const QMatrix4x4_Accessor &) m).flagBits <= 0x1; }
    static bool isScale(const QMatrix4x4 &m) { return ((const QMatrix4x4_Accessor &) m).flagBits <= 0x2; }
    static bool is2DSafe(const QMatrix4x4 &m) { return ((const QMatrix4x4_Accessor &) m).flagBits < 0x8; }
};

const float OPAQUE_LIMIT                = 0.999f;

ShaderManager::Shader *ShaderManager::prepareMaterial(QSGMaterial *material)
{
    QSGMaterialType *type = material->type();
    Shader *shader = rewrittenShaders.value(type, 0);
    if (shader)
        return shader;

    if (QSG_LOG_TIME_COMPILATION().isDebugEnabled())
        qsg_renderer_timer.start();
    Q_QUICK_SG_PROFILE_START(QQuickProfiler::SceneGraphContextFrame);

    QSGMaterialShader *s = material->createShader();
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    QSurfaceFormat::OpenGLContextProfile profile = ctx->format().profile();

    QOpenGLShaderProgram *p = s->program();
    char const *const *attr = s->attributeNames();
    int i;
    for (i = 0; attr[i]; ++i) {
        if (*attr[i])
            p->bindAttributeLocation(attr[i], i);
    }
    p->bindAttributeLocation("_qt_order", i);
    context->compileShader(s, material, qsgShaderRewriter_insertZAttributes(s->vertexShader(), profile), 0);
    context->initializeShader(s);
    if (!p->isLinked())
        return 0;

    shader = new Shader;
    shader->program = s;
    shader->pos_order = i;
    shader->id_zRange = p->uniformLocation("_qt_zRange");
    shader->lastOpacity = 0;

    Q_ASSERT(shader->pos_order >= 0);
    Q_ASSERT(shader->id_zRange >= 0);

    qCDebug(QSG_LOG_TIME_COMPILATION, "shader compiled in %dms", (int) qsg_renderer_timer.elapsed());

    Q_QUICK_SG_PROFILE_END(QQuickProfiler::SceneGraphContextFrame,
                           QQuickProfiler::SceneGraphContextMaterialCompile);

    rewrittenShaders[type] = shader;
    return shader;
}

ShaderManager::Shader *ShaderManager::prepareMaterialNoRewrite(QSGMaterial *material)
{
    QSGMaterialType *type = material->type();
    Shader *shader = stockShaders.value(type, 0);
    if (shader)
        return shader;

    if (QSG_LOG_TIME_COMPILATION().isDebugEnabled())
        qsg_renderer_timer.start();
    Q_QUICK_SG_PROFILE_START(QQuickProfiler::SceneGraphContextFrame);

    QSGMaterialShader *s = static_cast<QSGMaterialShader *>(material->createShader());
    context->compileShader(s, material);
    context->initializeShader(s);

    shader = new Shader();
    shader->program = s;
    shader->id_zRange = -1;
    shader->pos_order = -1;
    shader->lastOpacity = 0;

    stockShaders[type] = shader;

    qCDebug(QSG_LOG_TIME_COMPILATION, "shader compiled in %dms (no rewrite)", (int) qsg_renderer_timer.elapsed());

    Q_QUICK_SG_PROFILE_END(QQuickProfiler::SceneGraphContextFrame,
                           QQuickProfiler::SceneGraphContextMaterialCompile);
    return shader;
}

void ShaderManager::invalidated()
{
    qDeleteAll(stockShaders);
    stockShaders.clear();
    qDeleteAll(rewrittenShaders);
    rewrittenShaders.clear();
    delete blitProgram;
    blitProgram = 0;
}

void qsg_dumpShadowRoots(BatchRootInfo *i, int indent)
{
    static int extraIndent = 0;
    ++extraIndent;

    QByteArray ind(indent + extraIndent + 10, ' ');

    if (!i) {
        qDebug() << ind.constData() << "- no info";
    } else {
        qDebug() << ind.constData() << "- parent:" << i->parentRoot << "orders" << i->firstOrder << "->" << i->lastOrder << ", avail:" << i->availableOrders;
        for (QSet<Node *>::const_iterator it = i->subRoots.constBegin();
             it != i->subRoots.constEnd(); ++it) {
            qDebug() << ind.constData() << "-" << *it;
            qsg_dumpShadowRoots((*it)->rootInfo(), indent);
        }
    }

    --extraIndent;
}

void qsg_dumpShadowRoots(Node *n)
{
#ifndef QT_NO_DEBUG_OUTPUT
    static int indent = 0;
    ++indent;

    QByteArray ind(indent, ' ');

    if (n->type() == QSGNode::ClipNodeType || n->isBatchRoot) {
        qDebug() << ind.constData() << "[X]" << n->sgNode << hex << uint(n->sgNode->flags());
        qsg_dumpShadowRoots(n->rootInfo(), indent);
    } else {
        QDebug d = qDebug();
        d << ind.constData() << "[ ]" << n->sgNode << hex << uint(n->sgNode->flags());
        if (n->type() == QSGNode::GeometryNodeType)
            d << "order" << dec << n->element()->order;
    }

    SHADOWNODE_TRAVERSE(n)
            qsg_dumpShadowRoots(child);

    --indent;
#else
    Q_UNUSED(n)
#endif
}

Updater::Updater(Renderer *r)
    : renderer(r)
    , m_roots(32)
    , m_rootMatrices(8)
{
    m_roots.add(0);
    m_combined_matrix_stack.add(&m_identityMatrix);
    m_rootMatrices.add(m_identityMatrix);

    Q_ASSERT(sizeof(QMatrix4x4_Accessor) == sizeof(QMatrix4x4));
}

void Updater::updateStates(QSGNode *n)
{
    m_current_clip = 0;

    m_added = 0;
    m_transformChange = 0;
    m_opacityChange = 0;

    Node *sn = renderer->m_nodes.value(n, 0);
    Q_ASSERT(sn);

    if (Q_UNLIKELY(debug_roots()))
        qsg_dumpShadowRoots(sn);

    if (Q_UNLIKELY(debug_build())) {
        qDebug() << "Updater::updateStates()";
        if (sn->dirtyState & (QSGNode::DirtyNodeAdded << 16))
            qDebug() << " - nodes have been added";
        if (sn->dirtyState & (QSGNode::DirtyMatrix << 16))
            qDebug() << " - transforms have changed";
        if (sn->dirtyState & (QSGNode::DirtyOpacity << 16))
            qDebug() << " - opacity has changed";
        if (sn->dirtyState & (QSGNode::DirtyForceUpdate << 16))
            qDebug() << " - forceupdate";
    }

    if (Q_UNLIKELY(renderer->m_visualizeMode == Renderer::VisualizeChanges))
        renderer->visualizeChangesPrepare(sn);

    visitNode(sn);
}

void Updater::visitNode(Node *n)
{
    if (m_added == 0 && n->dirtyState == 0 && m_force_update == 0 && m_transformChange == 0 && m_opacityChange == 0)
        return;

    int count = m_added;
    if (n->dirtyState & QSGNode::DirtyNodeAdded)
        ++m_added;

    int force = m_force_update;
    if (n->dirtyState & QSGNode::DirtyForceUpdate)
        ++m_force_update;

    switch (n->type()) {
    case QSGNode::OpacityNodeType:
        visitOpacityNode(n);
        break;
    case QSGNode::TransformNodeType:
        visitTransformNode(n);
        break;
    case QSGNode::GeometryNodeType:
        visitGeometryNode(n);
        break;
    case QSGNode::ClipNodeType:
        visitClipNode(n);
        break;
    case QSGNode::RenderNodeType:
        if (m_added)
            n->renderNodeElement()->root = m_roots.last();
        // Fall through to visit children.
    default:
        SHADOWNODE_TRAVERSE(n) visitNode(child);
        break;
    }

    m_added = count;
    m_force_update = force;
    n->dirtyState = 0;
}

void Updater::visitClipNode(Node *n)
{
    ClipBatchRootInfo *extra = n->clipInfo();

    QSGClipNode *cn = static_cast<QSGClipNode *>(n->sgNode);

    if (m_roots.last() && m_added > 0)
        renderer->registerBatchRoot(n, m_roots.last());

    cn->setRendererClipList(m_current_clip);
    m_current_clip = cn;
    m_roots << n;
    m_rootMatrices.add(m_rootMatrices.last() * *m_combined_matrix_stack.last());
    extra->matrix = m_rootMatrices.last();
    cn->setRendererMatrix(&extra->matrix);
    m_combined_matrix_stack << &m_identityMatrix;

    SHADOWNODE_TRAVERSE(n) visitNode(child);

    m_current_clip = cn->clipList();
    m_rootMatrices.pop_back();
    m_combined_matrix_stack.pop_back();
    m_roots.pop_back();
}

void Updater::visitOpacityNode(Node *n)
{
    QSGOpacityNode *on = static_cast<QSGOpacityNode *>(n->sgNode);

    qreal combined = m_opacity_stack.last() * on->opacity();
    on->setCombinedOpacity(combined);
    m_opacity_stack.add(combined);

    if (m_added == 0 && n->dirtyState & QSGNode::DirtyOpacity) {
        bool was = n->isOpaque;
        bool is = on->opacity() > OPAQUE_LIMIT;
        if (was != is) {
            renderer->m_rebuild = Renderer::FullRebuild;
            n->isOpaque = is;
        }
        ++m_opacityChange;
        SHADOWNODE_TRAVERSE(n) visitNode(child);
        --m_opacityChange;
    } else {
        if (m_added > 0)
            n->isOpaque = on->opacity() > OPAQUE_LIMIT;
        SHADOWNODE_TRAVERSE(n) visitNode(child);
    }

    m_opacity_stack.pop_back();
}

void Updater::visitTransformNode(Node *n)
{
    bool popMatrixStack = false;
    bool popRootStack = false;
    bool dirty = n->dirtyState & QSGNode::DirtyMatrix;

    QSGTransformNode *tn = static_cast<QSGTransformNode *>(n->sgNode);

    if (n->isBatchRoot) {
        if (m_added > 0 && m_roots.last())
            renderer->registerBatchRoot(n, m_roots.last());
        tn->setCombinedMatrix(m_rootMatrices.last() * *m_combined_matrix_stack.last() * tn->matrix());

        // The only change in this subtree is ourselves and we are a batch root, so
        // only update subroots and return, saving tons of child-processing (flickable-panning)

        if (!n->becameBatchRoot && m_added == 0 && m_force_update == 0 && m_opacityChange == 0 && dirty && (n->dirtyState & ~QSGNode::DirtyMatrix) == 0) {
            BatchRootInfo *info = renderer->batchRootInfo(n);
            for (QSet<Node *>::const_iterator it = info->subRoots.constBegin();
                 it != info->subRoots.constEnd(); ++it) {
                updateRootTransforms(*it, n, tn->combinedMatrix());
            }
            return;
        }

        n->becameBatchRoot = false;

        m_combined_matrix_stack.add(&m_identityMatrix);
        m_roots.add(n);
        m_rootMatrices.add(tn->combinedMatrix());

        popMatrixStack = true;
        popRootStack = true;
    } else if (!tn->matrix().isIdentity()) {
        tn->setCombinedMatrix(*m_combined_matrix_stack.last() * tn->matrix());
        m_combined_matrix_stack.add(&tn->combinedMatrix());
        popMatrixStack = true;
    } else {
        tn->setCombinedMatrix(*m_combined_matrix_stack.last());
    }

    if (dirty)
        ++m_transformChange;

    SHADOWNODE_TRAVERSE(n) visitNode(child);

    if (dirty)
        --m_transformChange;
    if (popMatrixStack)
        m_combined_matrix_stack.pop_back();
    if (popRootStack) {
        m_roots.pop_back();
        m_rootMatrices.pop_back();
    }
}

void Updater::visitGeometryNode(Node *n)
{
    QSGGeometryNode *gn = static_cast<QSGGeometryNode *>(n->sgNode);

    gn->setRendererMatrix(m_combined_matrix_stack.last());
    gn->setRendererClipList(m_current_clip);
    gn->setInheritedOpacity(m_opacity_stack.last());

    if (m_added) {
        Element *e = n->element();
        e->root = m_roots.last();
        e->translateOnlyToRoot = QMatrix4x4_Accessor::isTranslate(*gn->matrix());

        if (e->root) {
            BatchRootInfo *info = renderer->batchRootInfo(e->root);
            while (info != 0) {
                info->availableOrders--;
                if (info->availableOrders < 0) {
                    renderer->m_rebuild |= Renderer::BuildRenderLists;
                } else {
                    renderer->m_rebuild |= Renderer::BuildRenderListsForTaggedRoots;
                    renderer->m_taggedRoots << e->root;
                }
                if (info->parentRoot != 0)
                    info = renderer->batchRootInfo(info->parentRoot);
                else
                    info = 0;
            }
        } else {
            renderer->m_rebuild |= Renderer::FullRebuild;
        }
    } else {
        if (m_transformChange) {
            Element *e = n->element();
            e->translateOnlyToRoot = QMatrix4x4_Accessor::isTranslate(*gn->matrix());
        }
        if (m_opacityChange) {
            Element *e = n->element();
            if (e->batch)
                renderer->invalidateBatchAndOverlappingRenderOrders(e->batch);
        }
    }

    SHADOWNODE_TRAVERSE(n) visitNode(child);
}

void Updater::updateRootTransforms(Node *node, Node *root, const QMatrix4x4 &combined)
{
    BatchRootInfo *info = renderer->batchRootInfo(node);
    QMatrix4x4 m;
    Node *n = node;

    while (n != root) {
        if (n->type() == QSGNode::TransformNodeType)
            m = static_cast<QSGTransformNode *>(n->sgNode)->matrix() * m;
        n = n->parent();
    }

    m = combined * m;

    if (node->type() == QSGNode::ClipNodeType) {
        static_cast<ClipBatchRootInfo *>(info)->matrix = m;
    } else {
        Q_ASSERT(node->type() == QSGNode::TransformNodeType);
        static_cast<QSGTransformNode *>(node->sgNode)->setCombinedMatrix(m);
    }

    for (QSet<Node *>::const_iterator it = info->subRoots.constBegin();
         it != info->subRoots.constEnd(); ++it) {
        updateRootTransforms(*it, node, m);
    }
}

int qsg_positionAttribute(QSGGeometry *g) {
    int vaOffset = 0;
    for (int a=0; a<g->attributeCount(); ++a) {
        const QSGGeometry::Attribute &attr = g->attributes()[a];
        if (attr.isVertexCoordinate && attr.tupleSize == 2 && attr.type == GL_FLOAT) {
            return vaOffset;
        }
        vaOffset += attr.tupleSize * size_of_type(attr.type);
    }
    return -1;
}


void Rect::map(const QMatrix4x4 &matrix)
{
    const float *m = matrix.constData();
    if (QMatrix4x4_Accessor::isScale(matrix)) {
        tl.x = tl.x * m[0] + m[12];
        tl.y = tl.y * m[5] + m[13];
        br.x = br.x * m[0] + m[12];
        br.y = br.y * m[5] + m[13];
        if (tl.x > br.x)
            qSwap(tl.x, br.x);
        if (tl.y > br.y)
            qSwap(tl.y, br.y);
    } else {
        Pt mtl = tl;
        Pt mtr = { br.x, tl.y };
        Pt mbl = { tl.x, br.y };
        Pt mbr = br;

        mtl.map(matrix);
        mtr.map(matrix);
        mbl.map(matrix);
        mbr.map(matrix);

        set(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
        (*this) |= mtl;
        (*this) |= mtr;
        (*this) |= mbl;
        (*this) |= mbr;
    }
}

void Element::computeBounds()
{
    Q_ASSERT(!boundsComputed);
    boundsComputed = true;

    QSGGeometry *g = node->geometry();
    int offset = qsg_positionAttribute(g);
    if (offset == -1) {
        // No position attribute means overlaps with everything..
        bounds.set(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX);
        return;
    }

    bounds.set(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    char *vd = (char *) g->vertexData() + offset;
    for (int i=0; i<g->vertexCount(); ++i) {
        bounds |= *(Pt *) vd;
        vd += g->sizeOfVertex();
    }
    bounds.map(*node->matrix());

    if (!qt_is_finite(bounds.tl.x) || bounds.tl.x == FLT_MAX)
        bounds.tl.x = -FLT_MAX;
    if (!qt_is_finite(bounds.tl.y) || bounds.tl.y == FLT_MAX)
        bounds.tl.y = -FLT_MAX;
    if (!qt_is_finite(bounds.br.x) || bounds.br.x == -FLT_MAX)
        bounds.br.x = FLT_MAX;
    if (!qt_is_finite(bounds.br.y) || bounds.br.y == -FLT_MAX)
        bounds.br.y = FLT_MAX;

    Q_ASSERT(bounds.tl.x <= bounds.br.x);
    Q_ASSERT(bounds.tl.y <= bounds.br.y);

    boundsOutsideFloatRange = bounds.isOutsideFloatRange();
}

BatchCompatibility Batch::isMaterialCompatible(Element *e) const
{
    Element *n = first;
    // Skip to the first node other than e which has not been removed
    while (n && (n == e || n->removed))
        n = n->nextInBatch;

    // Only 'e' in this batch, so a material change doesn't change anything as long as
    // its blending is still in sync with this batch...
    if (!n)
        return BatchIsCompatible;

    QSGMaterial *m = e->node->activeMaterial();
    QSGMaterial *nm = n->node->activeMaterial();
    return (nm->type() == m->type() && nm->compare(m) == 0)
            ? BatchIsCompatible
            : BatchBreaksOnCompare;
}

/*
 * Marks this batch as dirty or in the case where the geometry node has
 * changed to be incompatible with this batch, return false so that
 * the caller can mark the entire sg for a full rebuild...
 */
bool Batch::geometryWasChanged(QSGGeometryNode *gn)
{
    Element *e = first;
    Q_ASSERT_X(e, "Batch::geometryWasChanged", "Batch is expected to 'valid' at this time");
    // 'gn' is the first node in the batch, compare against the next one.
    while (e && (e->node == gn || e->removed))
        e = e->nextInBatch;
    if (!e || e->node->geometry()->attributes() == gn->geometry()->attributes()) {
        needsUpload = true;
        return true;
    } else {
        return false;
    }
}

void Batch::cleanupRemovedElements()
{
    // remove from front of batch..
    while (first && first->removed) {
        first = first->nextInBatch;
    }

    // Then continue and remove other nodes further out in the batch..
    if (first) {
        Element *e = first;
        while (e->nextInBatch) {
            if (e->nextInBatch->removed)
                e->nextInBatch = e->nextInBatch->nextInBatch;
            else
                e = e->nextInBatch;

        }
    }
}

/*
 * Iterates through all geometry nodes in this batch and unsets their batch,
 * thus forcing them to be rebuilt
 */
void Batch::invalidate()
{
    // If doing removal here is a performance issue, we might add a "hasRemoved" bit to
    // the batch to do an early out..
    cleanupRemovedElements();
    Element *e = first;
    first = 0;
    root = 0;
    while (e) {
        e->batch = 0;
        Element *n = e->nextInBatch;
        e->nextInBatch = 0;
        e = n;
    }
}

bool Batch::isTranslateOnlyToRoot() const {
    bool only = true;
    Element *e = first;
    while (e && only) {
        only &= e->translateOnlyToRoot;
        e = e->nextInBatch;
    }
    return only;
}

/*
 * Iterates through all the nodes in the batch and returns true if the
 * nodes are all safe to batch. There are two separate criteria:
 *
 * - The matrix is such that the z component of the result is of no
 *   consequence.
 *
 * - The bounds are inside the stable floating point range. This applies
 *   to desktop only where we in this case can trigger a fallback to
 *   unmerged in which case we pass the geometry straight through and
 *   just apply the matrix.
 *
 *   NOTE: This also means a slight performance impact for geometries which
 *   are defined to be outside the stable floating point range and still
 *   use single precision float, but given that this implicitly fixes
 *   huge lists and tables, it is worth it.
 */
bool Batch::isSafeToBatch() const {
    Element *e = first;
    while (e) {
        if (e->boundsOutsideFloatRange)
            return false;
        if (!QMatrix4x4_Accessor::is2DSafe(*e->node->matrix()))
            return false;
        e = e->nextInBatch;
    }
    return true;
}

static int qsg_countNodesInBatch(const Batch *batch)
{
    int sum = 0;
    Element *e = batch->first;
    while (e) {
        ++sum;
        e = e->nextInBatch;
    }
    return sum;
}

static int qsg_countNodesInBatches(const QDataBuffer<Batch *> &batches)
{
    int sum = 0;
    for (int i=0; i<batches.size(); ++i) {
        sum += qsg_countNodesInBatch(batches.at(i));
    }
    return sum;
}

Renderer::Renderer(QSGDefaultRenderContext *ctx)
    : QSGRenderer(ctx)
    , m_context(ctx)
    , m_opaqueRenderList(64)
    , m_alphaRenderList(64)
    , m_nextRenderOrder(0)
    , m_partialRebuild(false)
    , m_partialRebuildRoot(0)
    , m_useDepthBuffer(true)
    , m_opaqueBatches(16)
    , m_alphaBatches(16)
    , m_batchPool(16)
    , m_elementsToDelete(64)
    , m_tmpAlphaElements(16)
    , m_tmpOpaqueElements(16)
    , m_rebuild(FullRebuild)
    , m_zRange(0)
    , m_renderOrderRebuildLower(-1)
    , m_renderOrderRebuildUpper(-1)
    , m_currentMaterial(0)
    , m_currentShader(0)
    , m_currentStencilValue(0)
    , m_clipMatrixId(0)
    , m_currentClip(0)
    , m_currentClipType(NoClip)
    , m_vertexUploadPool(256)
#ifdef QSG_SEPARATE_INDEX_BUFFER
    , m_indexUploadPool(64)
#endif
    , m_vao(0)
    , m_visualizeMode(VisualizeNothing)
{
    initializeOpenGLFunctions();
    setNodeUpdater(new Updater(this));

    m_shaderManager = ctx->findChild<ShaderManager *>(QStringLiteral("__qt_ShaderManager"), Qt::FindDirectChildrenOnly);
    if (!m_shaderManager) {
        m_shaderManager = new ShaderManager(ctx);
        m_shaderManager->setObjectName(QStringLiteral("__qt_ShaderManager"));
        m_shaderManager->setParent(ctx);
        QObject::connect(ctx, SIGNAL(invalidated()), m_shaderManager, SLOT(invalidated()), Qt::DirectConnection);
    }

    m_bufferStrategy = GL_STATIC_DRAW;
    if (Q_UNLIKELY(qEnvironmentVariableIsSet("QSG_RENDERER_BUFFER_STRATEGY"))) {
        const QByteArray strategy = qgetenv("QSG_RENDERER_BUFFER_STRATEGY");
        if (strategy == "dynamic")
            m_bufferStrategy = GL_DYNAMIC_DRAW;
        else if (strategy == "stream")
            m_bufferStrategy = GL_STREAM_DRAW;
    }

    m_batchNodeThreshold = qt_sg_envInt("QSG_RENDERER_BATCH_NODE_THRESHOLD", 64);
    m_batchVertexThreshold = qt_sg_envInt("QSG_RENDERER_BATCH_VERTEX_THRESHOLD", 1024);

    if (Q_UNLIKELY(debug_build() || debug_render())) {
        qDebug() << "Batch thresholds: nodes:" << m_batchNodeThreshold << " vertices:" << m_batchVertexThreshold;
        qDebug() << "Using buffer strategy:" << (m_bufferStrategy == GL_STATIC_DRAW ? "static" : (m_bufferStrategy == GL_DYNAMIC_DRAW ? "dynamic" : "stream"));
    }

    // If rendering with an OpenGL Core profile context, we need to create a VAO
    // to hold our vertex specification state.
    if (m_context->openglContext()->format().profile() == QSurfaceFormat::CoreProfile) {
        m_vao = new QOpenGLVertexArrayObject(this);
        m_vao->create();
    }

    bool useDepth = qEnvironmentVariableIsEmpty("QSG_NO_DEPTH_BUFFER");
    m_useDepthBuffer = useDepth && ctx->openglContext()->format().depthBufferSize() > 0;
}

static void qsg_wipeBuffer(Buffer *buffer, QOpenGLFunctions *funcs)
{
    funcs->glDeleteBuffers(1, &buffer->id);
    // The free here is ok because we're in one of two situations.
    // 1. We're using the upload pool in which case unmap will have set the
    //    data pointer to 0 and calling free on 0 is ok.
    // 2. We're using dedicated buffers because of visualization or IBO workaround
    //    and the data something we malloced and must be freed.
    free(buffer->data);
}

static void qsg_wipeBatch(Batch *batch, QOpenGLFunctions *funcs)
{
    qsg_wipeBuffer(&batch->vbo, funcs);
#ifdef QSG_SEPARATE_INDEX_BUFFER
    qsg_wipeBuffer(&batch->ibo, funcs);
#endif
    delete batch;
}

Renderer::~Renderer()
{
    if (QOpenGLContext::currentContext()) {
        // Clean up batches and buffers
        for (int i=0; i<m_opaqueBatches.size(); ++i) qsg_wipeBatch(m_opaqueBatches.at(i), this);
        for (int i=0; i<m_alphaBatches.size(); ++i) qsg_wipeBatch(m_alphaBatches.at(i), this);
        for (int i=0; i<m_batchPool.size(); ++i) qsg_wipeBatch(m_batchPool.at(i), this);
    }

    for (Node *n : qAsConst(m_nodes))
        m_nodeAllocator.release(n);

    // Remaining elements...
    for (int i=0; i<m_elementsToDelete.size(); ++i) {
        Element *e = m_elementsToDelete.at(i);
        if (e->isRenderNode)
            delete static_cast<RenderNodeElement *>(e);
        else
            m_elementAllocator.release(e);
    }
}

void Renderer::invalidateAndRecycleBatch(Batch *b)
{
    b->invalidate();
    for (int i=0; i<m_batchPool.size(); ++i)
        if (b == m_batchPool.at(i))
            return;
    m_batchPool.add(b);
}

/* The code here does a CPU-side allocation which might seem like a performance issue
 * compared to using glMapBuffer or glMapBufferRange which would give me back
 * potentially GPU allocated memory and saving me one deep-copy, but...
 *
 * Because we do a lot of CPU-side transformations, we need random-access memory
 * and the memory returned from glMapBuffer/glMapBufferRange is typically
 * uncached and thus very slow for our purposes.
 *
 * ref: http://www.opengl.org/wiki/Buffer_Object
 */
void Renderer::map(Buffer *buffer, int byteSize, bool isIndexBuf)
{
    if (!m_context->hasBrokenIndexBufferObjects() && m_visualizeMode == VisualizeNothing) {
        // Common case, use a shared memory pool for uploading vertex data to avoid
        // excessive reevaluation
        QDataBuffer<char> &pool =
#ifdef QSG_SEPARATE_INDEX_BUFFER
                isIndexBuf ? m_indexUploadPool : m_vertexUploadPool;
#else
                m_vertexUploadPool;
        Q_UNUSED(isIndexBuf);
#endif
        if (byteSize > pool.size())
            pool.resize(byteSize);
        buffer->data = pool.data();
    } else if (buffer->size != byteSize) {
        free(buffer->data);
        buffer->data = (char *) malloc(byteSize);
    }
    buffer->size = byteSize;
}

void Renderer::unmap(Buffer *buffer, bool isIndexBuf)
{
    if (buffer->id == 0)
        glGenBuffers(1, &buffer->id);
    GLenum target = isIndexBuf ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    glBindBuffer(target, buffer->id);
    glBufferData(target, buffer->size, buffer->data, m_bufferStrategy);

    if (!m_context->hasBrokenIndexBufferObjects() && m_visualizeMode == VisualizeNothing) {
        buffer->data = 0;
    }
}

BatchRootInfo *Renderer::batchRootInfo(Node *node)
{
    BatchRootInfo *info = node->rootInfo();
    if (!info) {
        if (node->type() == QSGNode::ClipNodeType)
            info = new ClipBatchRootInfo;
        else {
            Q_ASSERT(node->type() == QSGNode::TransformNodeType);
            info = new BatchRootInfo;
        }
        node->data = info;
    }
    return info;
}

void Renderer::removeBatchRootFromParent(Node *childRoot)
{
    BatchRootInfo *childInfo = batchRootInfo(childRoot);
    if (!childInfo->parentRoot)
        return;
    BatchRootInfo *parentInfo = batchRootInfo(childInfo->parentRoot);

    Q_ASSERT(parentInfo->subRoots.contains(childRoot));
    parentInfo->subRoots.remove(childRoot);
    childInfo->parentRoot = 0;
}

void Renderer::registerBatchRoot(Node *subRoot, Node *parentRoot)
{
    BatchRootInfo *subInfo = batchRootInfo(subRoot);
    BatchRootInfo *parentInfo = batchRootInfo(parentRoot);
    subInfo->parentRoot = parentRoot;
    parentInfo->subRoots << subRoot;
}

bool Renderer::changeBatchRoot(Node *node, Node *root)
{
    BatchRootInfo *subInfo = batchRootInfo(node);
    if (subInfo->parentRoot == root)
        return false;
    if (subInfo->parentRoot) {
        BatchRootInfo *oldRootInfo = batchRootInfo(subInfo->parentRoot);
        oldRootInfo->subRoots.remove(node);
    }
    BatchRootInfo *newRootInfo = batchRootInfo(root);
    newRootInfo->subRoots << node;
    subInfo->parentRoot = root;
    return true;
}

void Renderer::nodeChangedBatchRoot(Node *node, Node *root)
{
    if (node->type() == QSGNode::ClipNodeType || node->isBatchRoot) {
        if (!changeBatchRoot(node, root))
            return;
        node = root;
    } else if (node->type() == QSGNode::GeometryNodeType) {
        // Only need to change the root as nodeChanged anyway flags a full update.
        Element *e = node->element();
        if (e) {
            e->root = root;
            e->boundsComputed = false;
        }
    }

    SHADOWNODE_TRAVERSE(node)
            nodeChangedBatchRoot(child, root);
}

void Renderer::nodeWasTransformed(Node *node, int *vertexCount)
{
    if (node->type() == QSGNode::GeometryNodeType) {
        QSGGeometryNode *gn = static_cast<QSGGeometryNode *>(node->sgNode);
        *vertexCount += gn->geometry()->vertexCount();
        Element *e  = node->element();
        if (e) {
            e->boundsComputed = false;
            if (e->batch) {
                if (!e->batch->isOpaque) {
                    invalidateBatchAndOverlappingRenderOrders(e->batch);
                } else if (e->batch->merged) {
                    e->batch->needsUpload = true;
                }
            }
        }
    }

    SHADOWNODE_TRAVERSE(node)
        nodeWasTransformed(child, vertexCount);
}

void Renderer::nodeWasAdded(QSGNode *node, Node *shadowParent)
{
    Q_ASSERT(!m_nodes.contains(node));
    if (node->isSubtreeBlocked())
        return;

    Node *snode = m_nodeAllocator.allocate();
    snode->sgNode = node;
    m_nodes.insert(node, snode);
    if (shadowParent)
        shadowParent->append(snode);

    if (node->type() == QSGNode::GeometryNodeType) {
        snode->data = m_elementAllocator.allocate();
        snode->element()->setNode(static_cast<QSGGeometryNode *>(node));

    } else if (node->type() == QSGNode::ClipNodeType) {
        snode->data = new ClipBatchRootInfo;
        m_rebuild |= FullRebuild;

    } else if (node->type() == QSGNode::RenderNodeType) {
        QSGRenderNode *rn = static_cast<QSGRenderNode *>(node);
        RenderNodeElement *e = new RenderNodeElement(rn);
        snode->data = e;
        Q_ASSERT(!m_renderNodeElements.contains(rn));
        m_renderNodeElements.insert(e->renderNode, e);
        if (!rn->flags().testFlag(QSGRenderNode::DepthAwareRendering))
            m_useDepthBuffer = false;
        m_rebuild |= FullRebuild;
    }

    QSGNODE_TRAVERSE(node)
            nodeWasAdded(child, snode);
}

void Renderer::nodeWasRemoved(Node *node)
{
    // Prefix traversal as removeBatchRootFromParent below removes nodes
    // in a bottom-up manner. Note that we *cannot* use SHADOWNODE_TRAVERSE
    // here, because we delete 'child' (when recursed, down below), so we'd
    // have a use-after-free.
    {
        Node *child = node->firstChild();
        while (child) {
            // Remove (and delete) child
            node->remove(child);
            nodeWasRemoved(child);
            child = node->firstChild();
        }
    }

    if (node->type() == QSGNode::GeometryNodeType) {
        Element *e = node->element();
        if (e) {
            e->removed = true;
            m_elementsToDelete.add(e);
            e->node = 0;
            if (e->root) {
                BatchRootInfo *info = batchRootInfo(e->root);
                info->availableOrders++;
            }
            if (e->batch) {
                e->batch->needsUpload = true;
            }

        }

    } else if (node->type() == QSGNode::ClipNodeType) {
        removeBatchRootFromParent(node);
        delete node->clipInfo();
        m_rebuild |= FullRebuild;
        m_taggedRoots.remove(node);

    } else if (node->isBatchRoot) {
        removeBatchRootFromParent(node);
        delete node->rootInfo();
        m_rebuild |= FullRebuild;
        m_taggedRoots.remove(node);

    } else if (node->type() == QSGNode::RenderNodeType) {
        RenderNodeElement *e = m_renderNodeElements.take(static_cast<QSGRenderNode *>(node->sgNode));
        if (e) {
            e->removed = true;
            m_elementsToDelete.add(e);

            if (m_renderNodeElements.isEmpty()) {
                static bool useDepth = qEnvironmentVariableIsEmpty("QSG_NO_DEPTH_BUFFER");
                m_useDepthBuffer = useDepth && m_context->openglContext()->format().depthBufferSize() > 0;
            }
        }
    }

    Q_ASSERT(m_nodes.contains(node->sgNode));

    m_nodeAllocator.release(m_nodes.take(node->sgNode));
}

void Renderer::turnNodeIntoBatchRoot(Node *node)
{
    if (Q_UNLIKELY(debug_change())) qDebug() << " - new batch root";
    m_rebuild |= FullRebuild;
    node->isBatchRoot = true;
    node->becameBatchRoot = true;

    Node *p = node->parent();
    while (p) {
        if (p->type() == QSGNode::ClipNodeType || p->isBatchRoot) {
            registerBatchRoot(node, p);
            break;
        }
        p = p->parent();
    }

    SHADOWNODE_TRAVERSE(node)
            nodeChangedBatchRoot(child, node);
}


void Renderer::nodeChanged(QSGNode *node, QSGNode::DirtyState state)
{
#ifndef QT_NO_DEBUG_OUTPUT
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
#endif
    // As this function calls nodeChanged recursively, we do it at the top
    // to avoid that any of the others are processed twice.
    if (state & QSGNode::DirtySubtreeBlocked) {
        Node *sn = m_nodes.value(node);
        bool blocked = node->isSubtreeBlocked();
        if (blocked && sn) {
            nodeChanged(node, QSGNode::DirtyNodeRemoved);
            Q_ASSERT(m_nodes.value(node) == 0);
        } else if (!blocked && !sn) {
            nodeChanged(node, QSGNode::DirtyNodeAdded);
        }
        return;
    }

    if (state & QSGNode::DirtyNodeAdded) {
        if (nodeUpdater()->isNodeBlocked(node, rootNode())) {
            QSGRenderer::nodeChanged(node, state);
            return;
        }
        if (node == rootNode())
            nodeWasAdded(node, 0);
        else
            nodeWasAdded(node, m_nodes.value(node->parent()));
    }

    // Mark this node dirty in the shadow tree.
    Node *shadowNode = m_nodes.value(node);

    // Blocked subtrees won't have shadow nodes, so we can safely abort
    // here..
    if (!shadowNode) {
        QSGRenderer::nodeChanged(node, state);
        return;
    }

    shadowNode->dirtyState |= state;

    if (state & QSGNode::DirtyMatrix && !shadowNode->isBatchRoot) {
        Q_ASSERT(node->type() == QSGNode::TransformNodeType);
        if (node->m_subtreeRenderableCount > m_batchNodeThreshold) {
            turnNodeIntoBatchRoot(shadowNode);
        } else {
            int vertices = 0;
            nodeWasTransformed(shadowNode, &vertices);
            if (vertices > m_batchVertexThreshold) {
                turnNodeIntoBatchRoot(shadowNode);
            }
        }
    }

    if (state & QSGNode::DirtyGeometry && node->type() == QSGNode::GeometryNodeType) {
        QSGGeometryNode *gn = static_cast<QSGGeometryNode *>(node);
        Element *e = shadowNode->element();
        if (e) {
            e->boundsComputed = false;
            Batch *b = e->batch;
            if (b) {
                if (!e->batch->geometryWasChanged(gn) || !e->batch->isOpaque) {
                    invalidateBatchAndOverlappingRenderOrders(e->batch);
                } else {
                    b->needsUpload = true;
                }
            }
        }
    }

    if (state & QSGNode::DirtyMaterial && node->type() == QSGNode::GeometryNodeType) {
        Element *e = shadowNode->element();
        if (e) {
            bool blended = hasMaterialWithBlending(static_cast<QSGGeometryNode *>(node));
            if (e->isMaterialBlended != blended) {
                m_rebuild |= Renderer::FullRebuild;
                e->isMaterialBlended = blended;
            } else if (e->batch) {
                if (e->batch->isMaterialCompatible(e) == BatchBreaksOnCompare)
                    invalidateBatchAndOverlappingRenderOrders(e->batch);
            } else {
                m_rebuild |= Renderer::BuildBatches;
            }
        }
    }

    // Mark the shadow tree dirty all the way back to the root...
    QSGNode::DirtyState dirtyChain = state & (QSGNode::DirtyNodeAdded
                                              | QSGNode::DirtyOpacity
                                              | QSGNode::DirtyMatrix
                                              | QSGNode::DirtySubtreeBlocked
                                              | QSGNode::DirtyForceUpdate);
    if (dirtyChain != 0) {
        dirtyChain = QSGNode::DirtyState(dirtyChain << 16);
        Node *sn = shadowNode->parent();
        while (sn) {
            sn->dirtyState |= dirtyChain;
            sn = sn->parent();
        }
    }

    // Delete happens at the very end because it deletes the shadownode.
    if (state & QSGNode::DirtyNodeRemoved) {
        Node *parent = shadowNode->parent();
        if (parent)
            parent->remove(shadowNode);
        nodeWasRemoved(shadowNode);
        Q_ASSERT(m_nodes.value(node) == 0);
    }

    QSGRenderer::nodeChanged(node, state);
}

/*
 * Traverses the tree and builds two list of geometry nodes. One for
 * the opaque and one for the translucent. These are populated
 * in the order they should visually appear in, meaning first
 * to the back and last to the front.
 *
 * We split opaque and translucent as we can perform different
 * types of reordering / batching strategies on them, depending
 *
 * Note: It would be tempting to use the shadow nodes instead of the QSGNodes
 * for traversal to avoid hash lookups, but the order of the children
 * is important and they are not preserved in the shadow tree, so we must
 * use the actual QSGNode tree.
 */
void Renderer::buildRenderLists(QSGNode *node)
{
    if (node->isSubtreeBlocked())
        return;

    Node *shadowNode = m_nodes.value(node);
    Q_ASSERT(shadowNode);

    if (node->type() == QSGNode::GeometryNodeType) {
        QSGGeometryNode *gn = static_cast<QSGGeometryNode *>(node);

        Element *e = shadowNode->element();
        Q_ASSERT(e);

        bool opaque = gn->inheritedOpacity() > OPAQUE_LIMIT && !(gn->activeMaterial()->flags() & QSGMaterial::Blending);
        if (opaque && m_useDepthBuffer)
            m_opaqueRenderList << e;
        else
            m_alphaRenderList << e;

        e->order = ++m_nextRenderOrder;
        // Used while rebuilding partial roots.
        if (m_partialRebuild)
            e->orphaned = false;

    } else if (node->type() == QSGNode::ClipNodeType || shadowNode->isBatchRoot) {
        Q_ASSERT(m_nodes.contains(node));
        BatchRootInfo *info = batchRootInfo(shadowNode);
        if (node == m_partialRebuildRoot) {
            m_nextRenderOrder = info->firstOrder;
            QSGNODE_TRAVERSE(node)
                    buildRenderLists(child);
            m_nextRenderOrder = info->lastOrder + 1;
        } else {
            int currentOrder = m_nextRenderOrder;
            QSGNODE_TRAVERSE(node)
                buildRenderLists(child);
            int padding = (m_nextRenderOrder - currentOrder) >> 2;
            info->firstOrder = currentOrder;
            info->availableOrders = padding;
            info->lastOrder = m_nextRenderOrder + padding;
            m_nextRenderOrder = info->lastOrder;
        }
        return;
    } else if (node->type() == QSGNode::RenderNodeType) {
        RenderNodeElement *e = shadowNode->renderNodeElement();
        m_alphaRenderList << e;
        e->order = ++m_nextRenderOrder;
        Q_ASSERT(e);
    }

    QSGNODE_TRAVERSE(node)
        buildRenderLists(child);
}

void Renderer::tagSubRoots(Node *node)
{
    BatchRootInfo *i = batchRootInfo(node);
    m_taggedRoots << node;
    for (QSet<Node *>::const_iterator it = i->subRoots.constBegin();
         it != i->subRoots.constEnd(); ++it) {
        tagSubRoots(*it);
    }
}

static void qsg_addOrphanedElements(QDataBuffer<Element *> &orphans, const QDataBuffer<Element *> &renderList)
{
    orphans.reset();
    for (int i=0; i<renderList.size(); ++i) {
        Element *e = renderList.at(i);
        if (e && !e->removed) {
            e->orphaned = true;
            orphans.add(e);
        }
    }
}

static void qsg_addBackOrphanedElements(QDataBuffer<Element *> &orphans, QDataBuffer<Element *> &renderList)
{
    for (int i=0; i<orphans.size(); ++i) {
        Element *e = orphans.at(i);
        if (e->orphaned)
            renderList.add(e);
    }
    orphans.reset();
}

/*
 * To rebuild the tagged roots, we start by putting all subroots of tagged
 * roots into the list of tagged roots. This is to make the rest of the
 * algorithm simpler.
 *
 * Second, we invalidate all batches which belong to tagged roots, which now
 * includes the entire subtree under a given root
 *
 * Then we call buildRenderLists for all tagged subroots which do not have
 * parents which are tagged, aka, we traverse only the topmosts roots.
 *
 * Then we sort the render lists based on their render order, to restore the
 * right order for rendering.
 */
void Renderer::buildRenderListsForTaggedRoots()
{
    // Flag any element that is currently in the render lists, but which
    // is not in a batch. This happens when we have a partial rebuild
    // in one sub tree while we have a BuildBatches change in another
    // isolated subtree. So that batch-building takes into account
    // these "orphaned" nodes, we flag them now. The ones under tagged
    // roots will be cleared again. The remaining ones are added into the
    // render lists so that they contain all visual nodes after the
    // function completes.
    qsg_addOrphanedElements(m_tmpOpaqueElements, m_opaqueRenderList);
    qsg_addOrphanedElements(m_tmpAlphaElements, m_alphaRenderList);

    // Take a copy now, as we will be adding to this while traversing..
    QSet<Node *> roots = m_taggedRoots;
    for (QSet<Node *>::const_iterator it = roots.constBegin();
         it != roots.constEnd(); ++it) {
        tagSubRoots(*it);
    }

    for (int i=0; i<m_opaqueBatches.size(); ++i) {
        Batch *b = m_opaqueBatches.at(i);
        if (m_taggedRoots.contains(b->root))
            invalidateAndRecycleBatch(b);

    }
    for (int i=0; i<m_alphaBatches.size(); ++i) {
        Batch *b = m_alphaBatches.at(i);
        if (m_taggedRoots.contains(b->root))
            invalidateAndRecycleBatch(b);
    }

    m_opaqueRenderList.reset();
    m_alphaRenderList.reset();
    int maxRenderOrder = m_nextRenderOrder;
    m_partialRebuild = true;
    // Traverse each root, assigning it
    for (QSet<Node *>::const_iterator it = m_taggedRoots.constBegin();
         it != m_taggedRoots.constEnd(); ++it) {
        Node *root = *it;
        BatchRootInfo *i = batchRootInfo(root);
        if ((!i->parentRoot || !m_taggedRoots.contains(i->parentRoot))
             && !nodeUpdater()->isNodeBlocked(root->sgNode, rootNode())) {
            m_nextRenderOrder = i->firstOrder;
            m_partialRebuildRoot = root->sgNode;
            buildRenderLists(root->sgNode);
        }
    }
    m_partialRebuild = false;
    m_partialRebuildRoot = 0;
    m_taggedRoots.clear();
    m_nextRenderOrder = qMax(m_nextRenderOrder, maxRenderOrder);

    // Add orphaned elements back into the list and then sort it..
    qsg_addBackOrphanedElements(m_tmpOpaqueElements, m_opaqueRenderList);
    qsg_addBackOrphanedElements(m_tmpAlphaElements, m_alphaRenderList);

    if (m_opaqueRenderList.size())
        std::sort(&m_opaqueRenderList.first(), &m_opaqueRenderList.last() + 1, qsg_sort_element_decreasing_order);
    if (m_alphaRenderList.size())
        std::sort(&m_alphaRenderList.first(), &m_alphaRenderList.last() + 1, qsg_sort_element_increasing_order);

}

void Renderer::buildRenderListsFromScratch()
{
    m_opaqueRenderList.reset();
    m_alphaRenderList.reset();

    for (int i=0; i<m_opaqueBatches.size(); ++i)
        invalidateAndRecycleBatch(m_opaqueBatches.at(i));
    for (int i=0; i<m_alphaBatches.size(); ++i)
        invalidateAndRecycleBatch(m_alphaBatches.at(i));
    m_opaqueBatches.reset();
    m_alphaBatches.reset();

    m_nextRenderOrder = 0;

    buildRenderLists(rootNode());
}

void Renderer::invalidateBatchAndOverlappingRenderOrders(Batch *batch)
{
    Q_ASSERT(batch);
    Q_ASSERT(batch->first);

    if (m_renderOrderRebuildLower < 0 || batch->first->order < m_renderOrderRebuildLower)
        m_renderOrderRebuildLower = batch->first->order;
    if (m_renderOrderRebuildUpper < 0 || batch->lastOrderInBatch > m_renderOrderRebuildUpper)
        m_renderOrderRebuildUpper = batch->lastOrderInBatch;

    batch->invalidate();

    for (int i=0; i<m_alphaBatches.size(); ++i) {
        Batch *b = m_alphaBatches.at(i);
        if (b->first) {
            int bf = b->first->order;
            int bl = b->lastOrderInBatch;
            if (bl > m_renderOrderRebuildLower && bf < m_renderOrderRebuildUpper)
                b->invalidate();
        }
    }

    m_rebuild |= BuildBatches;
}

/* Clean up batches by making it a consecutive list of "valid"
 * batches and moving all invalidated batches to the batches pool.
 */
void Renderer::cleanupBatches(QDataBuffer<Batch *> *batches) {
    if (batches->size()) {
        std::stable_sort(&batches->first(), &batches->last() + 1, qsg_sort_batch_is_valid);
        int count = 0;
        while (count < batches->size() && batches->at(count)->first)
            ++count;
        for (int i=count; i<batches->size(); ++i)
            invalidateAndRecycleBatch(batches->at(i));
        batches->resize(count);
    }
}

void Renderer::prepareOpaqueBatches()
{
    for (int i=m_opaqueRenderList.size() - 1; i >= 0; --i) {
        Element *ei = m_opaqueRenderList.at(i);
        if (!ei || ei->batch || ei->node->geometry()->vertexCount() == 0)
            continue;
        Batch *batch = newBatch();
        batch->first = ei;
        batch->root = ei->root;
        batch->isOpaque = true;
        batch->needsUpload = true;
        batch->positionAttribute = qsg_positionAttribute(ei->node->geometry());

        m_opaqueBatches.add(batch);

        ei->batch = batch;
        Element *next = ei;

        QSGGeometryNode *gni = ei->node;

        for (int j = i - 1; j >= 0; --j) {
            Element *ej = m_opaqueRenderList.at(j);
            if (!ej)
                continue;
            if (ej->root != ei->root)
                break;
            if (ej->batch || ej->node->geometry()->vertexCount() == 0)
                continue;

            QSGGeometryNode *gnj = ej->node;

            if (gni->clipList() == gnj->clipList()
                    && gni->geometry()->drawingMode() == gnj->geometry()->drawingMode()
                    && (gni->geometry()->drawingMode() != GL_LINES || gni->geometry()->lineWidth() == gnj->geometry()->lineWidth())
                    && gni->geometry()->attributes() == gnj->geometry()->attributes()
                    && gni->inheritedOpacity() == gnj->inheritedOpacity()
                    && gni->activeMaterial()->type() == gnj->activeMaterial()->type()
                    && gni->activeMaterial()->compare(gnj->activeMaterial()) == 0) {
                ej->batch = batch;
                next->nextInBatch = ej;
                next = ej;
            }
        }

        batch->lastOrderInBatch = next->order;
    }
}

bool Renderer::checkOverlap(int first, int last, const Rect &bounds)
{
    for (int i=first; i<=last; ++i) {
        Element *e = m_alphaRenderList.at(i);
        if (!e || e->batch)
            continue;
        Q_ASSERT(e->boundsComputed);
        if (e->bounds.intersects(bounds))
            return true;
    }
    return false;
}

/*
 *
 * To avoid the O(n^2) checkOverlap check in most cases, we have the
 * overlapBounds which is the union of all bounding rects to check overlap
 * for. We know that if it does not overlap, then none of the individual
 * ones will either. For the typical list case, this results in no calls
 * to checkOverlap what-so-ever. This also ensures that when all consecutive
 * items are matching (such as a table of text), we don't build up an
 * overlap bounds and thus do not require full overlap checks.
 */

void Renderer::prepareAlphaBatches()
{
    for (int i=0; i<m_alphaRenderList.size(); ++i) {
        Element *e = m_alphaRenderList.at(i);
        if (!e || e->isRenderNode)
            continue;
        Q_ASSERT(!e->removed);
        e->ensureBoundsValid();
    }

    for (int i=0; i<m_alphaRenderList.size(); ++i) {
        Element *ei = m_alphaRenderList.at(i);
        if (!ei || ei->batch)
            continue;

        if (ei->isRenderNode) {
            Batch *rnb = newBatch();
            rnb->first = ei;
            rnb->root = ei->root;
            rnb->isOpaque = false;
            rnb->isRenderNode = true;
            ei->batch = rnb;
            m_alphaBatches.add(rnb);
            continue;
        }

        if (ei->node->geometry()->vertexCount() == 0)
            continue;

        Batch *batch = newBatch();
        batch->first = ei;
        batch->root = ei->root;
        batch->isOpaque = false;
        batch->needsUpload = true;
        m_alphaBatches.add(batch);
        ei->batch = batch;

        QSGGeometryNode *gni = ei->node;
        batch->positionAttribute = qsg_positionAttribute(gni->geometry());

        Rect overlapBounds;
        overlapBounds.set(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);

        Element *next = ei;

        for (int j = i + 1; j < m_alphaRenderList.size(); ++j) {
            Element *ej = m_alphaRenderList.at(j);
            if (!ej)
                continue;
            if (ej->root != ei->root || ej->isRenderNode)
                break;
            if (ej->batch)
                continue;

            QSGGeometryNode *gnj = ej->node;
            if (gnj->geometry()->vertexCount() == 0)
                continue;

            if (gni->clipList() == gnj->clipList()
                    && gni->geometry()->drawingMode() == gnj->geometry()->drawingMode()
                    && (gni->geometry()->drawingMode() != GL_LINES || gni->geometry()->lineWidth() == gnj->geometry()->lineWidth())
                    && gni->geometry()->attributes() == gnj->geometry()->attributes()
                    && gni->inheritedOpacity() == gnj->inheritedOpacity()
                    && gni->activeMaterial()->type() == gnj->activeMaterial()->type()
                    && gni->activeMaterial()->compare(gnj->activeMaterial()) == 0) {
                if (!overlapBounds.intersects(ej->bounds) || !checkOverlap(i+1, j - 1, ej->bounds)) {
                    ej->batch = batch;
                    next->nextInBatch = ej;
                    next = ej;
                } else {
                    /* When we come across a compatible element which hits an overlap, we
                     * need to stop the batch right away. We cannot add more elements
                     * to the current batch as they will be rendered before the batch that the
                     * current 'ej' will be added to.
                     */
                    break;
                }
            } else {
                overlapBounds |= ej->bounds;
            }
        }

        batch->lastOrderInBatch = next->order;
    }


}

static inline int qsg_fixIndexCount(int iCount, GLenum drawMode) {
    switch (drawMode) {
    case GL_TRIANGLE_STRIP:
        // Merged triangle strips need to contain degenerate triangles at the beginning and end.
        // One could save 2 uploaded ushorts here by ditching the padding for the front of the
        // first and the end of the last, but for simplicity, we simply don't care.
        // Those extra triangles will be skipped while drawing to preserve the strip's parity
        // anyhow.
        return iCount + 2;
    case GL_LINES:
        // For lines we drop the last vertex if the number of vertices is uneven.
        return iCount - (iCount % 2);
    case GL_TRIANGLES:
        // For triangles we drop trailing vertices until the result is divisible by 3.
        return iCount - (iCount % 3);
    default:
        return iCount;
    }
}

/* These parameters warrant some explanation...
 *
 * vaOffset: The byte offset into the vertex data to the location of the
 *           2D float point vertex attributes.
 *
 * vertexData: destination where the geometry's vertex data should go
 *
 * zData: destination of geometries injected Z positioning
 *
 * indexData: destination of the indices for this element
 *
 * iBase: The starting index for this element in the batch
 */

void Renderer::uploadMergedElement(Element *e, int vaOffset, char **vertexData, char **zData, char **indexData, quint16 *iBase, int *indexCount)
{
    if (Q_UNLIKELY(debug_upload())) qDebug() << "  - uploading element:" << e << e->node << (void *) *vertexData << (qintptr) (*zData - *vertexData) << (qintptr) (*indexData - *vertexData);
    QSGGeometry *g = e->node->geometry();

    const QMatrix4x4 &localx = *e->node->matrix();

    const int vCount = g->vertexCount();
    const int vSize = g->sizeOfVertex();
    memcpy(*vertexData, g->vertexData(), vSize * vCount);

    // apply vertex transform..
    char *vdata = *vertexData + vaOffset;
    if (((const QMatrix4x4_Accessor &) localx).flagBits == 1) {
        for (int i=0; i<vCount; ++i) {
            Pt *p = (Pt *) vdata;
            p->x += ((const QMatrix4x4_Accessor &) localx).m[3][0];
            p->y += ((const QMatrix4x4_Accessor &) localx).m[3][1];
            vdata += vSize;
        }
    } else if (((const QMatrix4x4_Accessor &) localx).flagBits > 1) {
        for (int i=0; i<vCount; ++i) {
            ((Pt *) vdata)->map(localx);
            vdata += vSize;
        }
    }

    if (m_useDepthBuffer) {
        float *vzorder = (float *) *zData;
        float zorder = 1.0f - e->order * m_zRange;
        for (int i=0; i<vCount; ++i)
            vzorder[i] = zorder;
        *zData += vCount * sizeof(float);
    }

    int iCount = g->indexCount();
    quint16 *indices = (quint16 *) *indexData;

    if (iCount == 0) {
        iCount = vCount;
        if (g->drawingMode() == GL_TRIANGLE_STRIP)
            *indices++ = *iBase;
        else
            iCount = qsg_fixIndexCount(iCount, g->drawingMode());

        for (int i=0; i<iCount; ++i)
            indices[i] = *iBase + i;
    } else {
        const quint16 *srcIndices = g->indexDataAsUShort();
        if (g->drawingMode() == GL_TRIANGLE_STRIP)
            *indices++ = *iBase + srcIndices[0];
        else
            iCount = qsg_fixIndexCount(iCount, g->drawingMode());

        for (int i=0; i<iCount; ++i)
            indices[i] = *iBase + srcIndices[i];
    }
    if (g->drawingMode() == GL_TRIANGLE_STRIP) {
        indices[iCount] = indices[iCount - 1];
        iCount += 2;
    }

    *vertexData += vCount * vSize;
    *indexData += iCount * sizeof(quint16);
    *iBase += vCount;
    *indexCount += iCount;
}

static QMatrix4x4 qsg_matrixForRoot(Node *node)
{
    if (node->type() == QSGNode::TransformNodeType)
        return static_cast<QSGTransformNode *>(node->sgNode)->combinedMatrix();
    Q_ASSERT(node->type() == QSGNode::ClipNodeType);
    QSGClipNode *c = static_cast<QSGClipNode *>(node->sgNode);
    return *c->matrix();
}

void Renderer::uploadBatch(Batch *b)
{
        // Early out if nothing has changed in this batch..
        if (!b->needsUpload) {
            if (Q_UNLIKELY(debug_upload())) qDebug() << " Batch:" << b << "already uploaded...";
            return;
        }

        if (!b->first) {
            if (Q_UNLIKELY(debug_upload())) qDebug() << " Batch:" << b << "is invalid...";
            return;
        }

        if (b->isRenderNode) {
            if (Q_UNLIKELY(debug_upload())) qDebug() << " Batch: " << b << "is a render node...";
            return;
        }

        // Figure out if we can merge or not, if not, then just render the batch as is..
        Q_ASSERT(b->first);
        Q_ASSERT(b->first->node);

        QSGGeometryNode *gn = b->first->node;
        QSGGeometry *g =  gn->geometry();
        QSGMaterial::Flags flags = gn->activeMaterial()->flags();
        bool canMerge = (g->drawingMode() == GL_TRIANGLES || g->drawingMode() == GL_TRIANGLE_STRIP ||
                         g->drawingMode() == GL_LINES || g->drawingMode() == GL_POINTS)
                        && b->positionAttribute >= 0
                        && g->indexType() == GL_UNSIGNED_SHORT
                        && (flags & (QSGMaterial::CustomCompileStep | QSGMaterial_FullMatrix)) == 0
                        && ((flags & QSGMaterial::RequiresFullMatrixExceptTranslate) == 0 || b->isTranslateOnlyToRoot())
                        && b->isSafeToBatch();

        b->merged = canMerge;

        // Figure out how much memory we need...
        b->vertexCount = 0;
        b->indexCount = 0;
        int unmergedIndexSize = 0;
        Element *e = b->first;

        while (e) {
            QSGGeometry *eg = e->node->geometry();
            b->vertexCount += eg->vertexCount();
            int iCount = eg->indexCount();
            if (b->merged) {
                if (iCount == 0)
                    iCount = eg->vertexCount();
                iCount = qsg_fixIndexCount(iCount, g->drawingMode());
            } else {
                unmergedIndexSize += iCount * eg->sizeOfIndex();
            }
            b->indexCount += iCount;
            e = e->nextInBatch;
        }

        // Abort if there are no vertices in this batch.. We abort this late as
        // this is a broken usecase which we do not care to optimize for...
        if (b->vertexCount == 0 || (b->merged && b->indexCount == 0))
            return;

        /* Allocate memory for this batch. Merged batches are divided into three separate blocks
           1. Vertex data for all elements, as they were in the QSGGeometry object, but
              with the tranform relative to this batch's root applied. The vertex data
              is otherwise unmodified.
           2. Z data for all elements, derived from each elements "render order".
              This is present for merged data only.
           3. Indices for all elements, as they were in the QSGGeometry object, but
              adjusted so that each index matches its.
              And for TRIANGLE_STRIPs, we need to insert degenerate between each
              primitive. These are unsigned shorts for merged and arbitrary for
              non-merged.
         */
        int bufferSize =  b->vertexCount * g->sizeOfVertex();
        int ibufferSize = 0;
        if (b->merged) {
            ibufferSize = b->indexCount * sizeof(quint16);
            if (m_useDepthBuffer)
                bufferSize += b->vertexCount * sizeof(float);
        } else {
            ibufferSize = unmergedIndexSize;
        }

#ifdef QSG_SEPARATE_INDEX_BUFFER
        map(&b->ibo, ibufferSize, true);
#else
        bufferSize += ibufferSize;
#endif
        map(&b->vbo, bufferSize);

        if (Q_UNLIKELY(debug_upload())) qDebug() << " - batch" << b << " first:" << b->first << " root:"
                                   << b->root << " merged:" << b->merged << " positionAttribute" << b->positionAttribute
                                   << " vbo:" << b->vbo.id << ":" << b->vbo.size;

        if (b->merged) {
            char *vertexData = b->vbo.data;
            char *zData = vertexData + b->vertexCount * g->sizeOfVertex();
#ifdef QSG_SEPARATE_INDEX_BUFFER
            char *indexData = b->ibo.data;
#else
            char *indexData = zData + (m_useDepthBuffer ? b->vertexCount * sizeof(float) : 0);
#endif

            quint16 iOffset = 0;
            e = b->first;
            int verticesInSet = 0;
            int indicesInSet = 0;
            b->drawSets.reset();
#ifdef QSG_SEPARATE_INDEX_BUFFER
            int drawSetIndices = 0;
#else
            int drawSetIndices = indexData - vertexData;
#endif
            b->drawSets << DrawSet(0, zData - vertexData, drawSetIndices);
            while (e) {
                verticesInSet  += e->node->geometry()->vertexCount();
                if (verticesInSet > 0xffff) {
                    b->drawSets.last().indexCount = indicesInSet;
                    if (g->drawingMode() == GL_TRIANGLE_STRIP) {
                        b->drawSets.last().indices += 1 * sizeof(quint16);
                        b->drawSets.last().indexCount -= 2;
                    }
#ifdef QSG_SEPARATE_INDEX_BUFFER
                    drawSetIndices = indexData - b->ibo.data;
#else
                    drawSetIndices = indexData - b->vbo.data;
#endif
                    b->drawSets << DrawSet(vertexData - b->vbo.data,
                                           zData - b->vbo.data,
                                           drawSetIndices);
                    iOffset = 0;
                    verticesInSet = e->node->geometry()->vertexCount();
                    indicesInSet = 0;
                }
                uploadMergedElement(e, b->positionAttribute, &vertexData, &zData, &indexData, &iOffset, &indicesInSet);
                e = e->nextInBatch;
            }
            b->drawSets.last().indexCount = indicesInSet;
            // We skip the very first and very last degenerate triangles since they aren't needed
            // and the first one would reverse the vertex ordering of the merged strips.
            if (g->drawingMode() == GL_TRIANGLE_STRIP) {
                b->drawSets.last().indices += 1 * sizeof(quint16);
                b->drawSets.last().indexCount -= 2;
            }
        } else {
            char *vboData = b->vbo.data;
#ifdef QSG_SEPARATE_INDEX_BUFFER
            char *iboData = b->ibo.data;
#else
            char *iboData = vboData + b->vertexCount * g->sizeOfVertex();
#endif
            Element *e = b->first;
            while (e) {
                QSGGeometry *g = e->node->geometry();
                int vbs = g->vertexCount() * g->sizeOfVertex();
                memcpy(vboData, g->vertexData(), vbs);
                vboData = vboData + vbs;
                if (g->indexCount()) {
                    int ibs = g->indexCount() * g->sizeOfIndex();
                    memcpy(iboData, g->indexData(), ibs);
                    iboData += ibs;
                }
                e = e->nextInBatch;
            }
        }
#ifndef QT_NO_DEBUG_OUTPUT
        if (Q_UNLIKELY(debug_upload())) {
            const char *vd = b->vbo.data;
            qDebug() << "  -- Vertex Data, count:" << b->vertexCount << " - " << g->sizeOfVertex() << "bytes/vertex";
            for (int i=0; i<b->vertexCount; ++i) {
                QDebug dump = qDebug().nospace();
                dump << "  --- " << i << ": ";
                int offset = 0;
                for (int a=0; a<g->attributeCount(); ++a) {
                    const QSGGeometry::Attribute &attr = g->attributes()[a];
                    dump << attr.position << ":(" << attr.tupleSize << ",";
                    if (attr.type == GL_FLOAT) {
                        dump << "float ";
                        if (attr.isVertexCoordinate)
                            dump << "* ";
                        for (int t=0; t<attr.tupleSize; ++t)
                            dump << *(const float *)(vd + offset + t * sizeof(float)) << " ";
                    } else if (attr.type == GL_UNSIGNED_BYTE) {
                        dump << "ubyte ";
                        for (int t=0; t<attr.tupleSize; ++t)
                            dump << *(const unsigned char *)(vd + offset + t * sizeof(unsigned char)) << " ";
                    }
                    dump << ") ";
                    offset += attr.tupleSize * size_of_type(attr.type);
                }
                if (b->merged && m_useDepthBuffer) {
                    float zorder = ((float*)(b->vbo.data + b->vertexCount * g->sizeOfVertex()))[i];
                    dump << " Z:(" << zorder << ")";
                }
                vd += g->sizeOfVertex();
            }

            if (!b->drawSets.isEmpty()) {
                const quint16 *id =
# ifdef QSG_SEPARATE_INDEX_BUFFER
                    (const quint16 *) (b->ibo.data);
# else
                    (const quint16 *) (b->vbo.data + b->drawSets.at(0).indices);
# endif
                {
                    QDebug iDump = qDebug();
                    iDump << "  -- Index Data, count:" << b->indexCount;
                    for (int i=0; i<b->indexCount; ++i) {
                        if ((i % 24) == 0)
                           iDump << endl << "  --- ";
                        iDump << id[i];
                    }
                }

                for (int i=0; i<b->drawSets.size(); ++i) {
                    const DrawSet &s = b->drawSets.at(i);
                    qDebug() << "  -- DrawSet: indexCount:" << s.indexCount << " vertices:" << s.vertices << " z:" << s.zorders << " indices:" << s.indices;
                }
            }
        }
#endif // QT_NO_DEBUG_OUTPUT

        unmap(&b->vbo);
#ifdef QSG_SEPARATE_INDEX_BUFFER
        unmap(&b->ibo, true);
#endif

        if (Q_UNLIKELY(debug_upload())) qDebug() << "  --- vertex/index buffers unmapped, batch upload completed...";

        b->needsUpload = false;

        if (Q_UNLIKELY(debug_render()))
            b->uploadedThisFrame = true;
}

/*!
 * Convenience function to set up the stencil buffer for clipping based on \a clip.
 *
 * If the clip is a pixel aligned rectangle, this function will use glScissor instead
 * of stencil.
 */
Renderer::ClipType Renderer::updateStencilClip(const QSGClipNode *clip)
{
    if (!clip) {
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_SCISSOR_TEST);
        return NoClip;
    }

    ClipType clipType = NoClip;

    glDisable(GL_SCISSOR_TEST);

    m_currentStencilValue = 0;
    m_currentScissorRect = QRect();
    while (clip) {
        QMatrix4x4 m = m_current_projection_matrix;
        if (clip->matrix())
            m *= *clip->matrix();

        // TODO: Check for multisampling and pixel grid alignment.
        bool isRectangleWithNoPerspective = clip->isRectangular()
                && qFuzzyIsNull(m(3, 0)) && qFuzzyIsNull(m(3, 1));
        bool noRotate = qFuzzyIsNull(m(0, 1)) && qFuzzyIsNull(m(1, 0));
        bool isRotate90 = qFuzzyIsNull(m(0, 0)) && qFuzzyIsNull(m(1, 1));

        if (isRectangleWithNoPerspective && (noRotate || isRotate90)) {
            QRectF bbox = clip->clipRect();
            qreal invW = 1 / m(3, 3);
            qreal fx1, fy1, fx2, fy2;
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

            QRect deviceRect = this->deviceRect();

            GLint ix1 = qRound((fx1 + 1) * deviceRect.width() * qreal(0.5));
            GLint iy1 = qRound((fy1 + 1) * deviceRect.height() * qreal(0.5));
            GLint ix2 = qRound((fx2 + 1) * deviceRect.width() * qreal(0.5));
            GLint iy2 = qRound((fy2 + 1) * deviceRect.height() * qreal(0.5));

            if (!(clipType & ScissorClip)) {
                m_currentScissorRect = QRect(ix1, iy1, ix2 - ix1, iy2 - iy1);
                glEnable(GL_SCISSOR_TEST);
                clipType |= ScissorClip;
            } else {
                m_currentScissorRect &= QRect(ix1, iy1, ix2 - ix1, iy2 - iy1);
            }
            glScissor(m_currentScissorRect.x(), m_currentScissorRect.y(),
                      m_currentScissorRect.width(), m_currentScissorRect.height());
        } else {
            if (!(clipType & StencilClip)) {
                if (!m_clipProgram.isLinked()) {
                    QSGShaderSourceBuilder::initializeProgramFromFiles(
                        &m_clipProgram,
                        QStringLiteral(":/qt-project.org/scenegraph/shaders/stencilclip.vert"),
                        QStringLiteral(":/qt-project.org/scenegraph/shaders/stencilclip.frag"));
                    m_clipProgram.bindAttributeLocation("vCoord", 0);
                    m_clipProgram.link();
                    m_clipMatrixId = m_clipProgram.uniformLocation("matrix");
                }

                glClearStencil(0);
                glClear(GL_STENCIL_BUFFER_BIT);
                glEnable(GL_STENCIL_TEST);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glDepthMask(GL_FALSE);

                m_clipProgram.bind();
                m_clipProgram.enableAttributeArray(0);

                clipType |= StencilClip;
            }

            glStencilFunc(GL_EQUAL, m_currentStencilValue, 0xff); // stencil test, ref, test mask
            glStencilOp(GL_KEEP, GL_KEEP, GL_INCR); // stencil fail, z fail, z pass

            const QSGGeometry *g = clip->geometry();
            Q_ASSERT(g->attributeCount() > 0);
            const QSGGeometry::Attribute *a = g->attributes();
            glVertexAttribPointer(0, a->tupleSize, a->type, GL_FALSE, g->sizeOfVertex(), g->vertexData());

            m_clipProgram.setUniformValue(m_clipMatrixId, m);
            if (g->indexCount()) {
                glDrawElements(g->drawingMode(), g->indexCount(), g->indexType(), g->indexData());
            } else {
                glDrawArrays(g->drawingMode(), 0, g->vertexCount());
            }

            ++m_currentStencilValue;
        }

        clip = clip->clipList();
    }

    if (clipType & StencilClip) {
        m_clipProgram.disableAttributeArray(0);
        glStencilFunc(GL_EQUAL, m_currentStencilValue, 0xff); // stencil test, ref, test mask
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); // stencil fail, z fail, z pass
        bindable()->reactivate();
    } else {
        glDisable(GL_STENCIL_TEST);
    }

    return clipType;
}

void Renderer::updateClip(const QSGClipNode *clipList, const Batch *batch)
{
    if (clipList != m_currentClip && Q_LIKELY(!debug_noclip())) {
        m_currentClip = clipList;
        // updateClip sets another program, so force-reactivate our own
        if (m_currentShader)
            setActiveShader(0, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        if (batch->isOpaque)
            glDisable(GL_DEPTH_TEST);
        m_currentClipType = updateStencilClip(m_currentClip);
        if (batch->isOpaque) {
            glEnable(GL_DEPTH_TEST);
            if (m_currentClipType & StencilClip)
                glDepthMask(true);
        }
    }
}

/*!
 * Look at the attribute arrays and potentially the injected z attribute to figure out
 * which vertex attribute arrays need to be enabled and not. Then update the current
 * Shader and current QSGMaterialShader.
 */
void Renderer::setActiveShader(QSGMaterialShader *program, ShaderManager::Shader *shader)
{
    const char * const *c = m_currentProgram ? m_currentProgram->attributeNames() : 0;
    const char * const *n = program ? program->attributeNames() : 0;

    int cza = m_currentShader ? m_currentShader->pos_order : -1;
    int nza = shader ? shader->pos_order : -1;

    int i = 0;
    while (c || n) {

        bool was = c;
        if (cza == i) {
            was = true;
            c = 0;
        } else if (c && !c[i]) { // end of the attribute array names
            c = 0;
            was = false;
        }

        bool is = n;
        if (nza == i) {
            is = true;
            n = 0;
        } else if (n && !n[i]) {
            n = 0;
            is = false;
        }

        if (is && !was)
            glEnableVertexAttribArray(i);
        else if (was && !is)
            glDisableVertexAttribArray(i);

        ++i;
    }

    if (m_currentProgram)
        m_currentProgram->deactivate();
    m_currentProgram = program;
    m_currentShader = shader;
    m_currentMaterial = 0;
    if (m_currentProgram) {
        m_currentProgram->program()->bind();
        m_currentProgram->activate();
    }
}

void Renderer::renderMergedBatch(const Batch *batch)
{
    if (batch->vertexCount == 0 || batch->indexCount == 0)
        return;

    Element *e = batch->first;
    Q_ASSERT(e);

#ifndef QT_NO_DEBUG_OUTPUT
    if (Q_UNLIKELY(debug_render())) {
        QDebug debug = qDebug();
        debug << " -"
              << batch
              << (batch->uploadedThisFrame ? "[  upload]" : "[retained]")
              << (e->node->clipList() ? "[  clip]" : "[noclip]")
              << (batch->isOpaque ? "[opaque]" : "[ alpha]")
              << "[  merged]"
              << " Nodes:" << QString::fromLatin1("%1").arg(qsg_countNodesInBatch(batch), 4).toLatin1().constData()
              << " Vertices:" << QString::fromLatin1("%1").arg(batch->vertexCount, 5).toLatin1().constData()
              << " Indices:" << QString::fromLatin1("%1").arg(batch->indexCount, 5).toLatin1().constData()
              << " root:" << batch->root;
        if (batch->drawSets.size() > 1)
            debug << "sets:" << batch->drawSets.size();
        if (!batch->isOpaque)
            debug << "opacity:" << e->node->inheritedOpacity();
        batch->uploadedThisFrame = false;
    }
#endif

    QSGGeometryNode *gn = e->node;

    // We always have dirty matrix as all batches are at a unique z range.
    QSGMaterialShader::RenderState::DirtyStates dirty = QSGMaterialShader::RenderState::DirtyMatrix;
    if (batch->root)
        m_current_model_view_matrix = qsg_matrixForRoot(batch->root);
    else
        m_current_model_view_matrix.setToIdentity();
    m_current_determinant = m_current_model_view_matrix.determinant();
    m_current_projection_matrix = projectionMatrix(); // has potentially been changed by renderUnmergedBatch..

    // updateClip() uses m_current_projection_matrix.
    updateClip(gn->clipList(), batch);

    glBindBuffer(GL_ARRAY_BUFFER, batch->vbo.id);

    char *indexBase = 0;
#ifdef QSG_SEPARATE_INDEX_BUFFER
    const Buffer *indexBuf = &batch->ibo;
#else
    const Buffer *indexBuf = &batch->vbo;
#endif
    if (m_context->hasBrokenIndexBufferObjects()) {
        indexBase = indexBuf->data;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    } else {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf->id);
    }


    QSGMaterial *material = gn->activeMaterial();
    ShaderManager::Shader *sms = m_useDepthBuffer ? m_shaderManager->prepareMaterial(material) : m_shaderManager->prepareMaterialNoRewrite(material);
    if (!sms)
        return;
    QSGMaterialShader *program = sms->program;

    if (m_currentShader != sms)
        setActiveShader(program, sms);

    m_current_opacity = gn->inheritedOpacity();
    if (sms->lastOpacity != m_current_opacity) {
        dirty |= QSGMaterialShader::RenderState::DirtyOpacity;
        sms->lastOpacity = m_current_opacity;
    }

    program->updateState(state(dirty), material, m_currentMaterial);

#ifndef QT_NO_DEBUG
    if (qsg_test_and_clear_material_failure()) {
        qDebug() << "QSGMaterial::updateState triggered an error (merged), batch will be skipped:";
        Element *ee = e;
        while (ee) {
            qDebug() << "   -" << ee->node;
            ee = ee->nextInBatch;
        }
        QSGNodeDumper::dump(rootNode());
        qFatal("Aborting: scene graph is invalid...");
    }
#endif

    m_currentMaterial = material;

    QSGGeometry* g = gn->geometry();
    updateLineWidth(g);
    char const *const *attrNames = program->attributeNames();
    for (int i=0; i<batch->drawSets.size(); ++i) {
        const DrawSet &draw = batch->drawSets.at(i);
        int offset = 0;
        for (int j = 0; attrNames[j]; ++j) {
            if (!*attrNames[j])
                continue;
            const QSGGeometry::Attribute &a = g->attributes()[j];
            GLboolean normalize = a.type != GL_FLOAT && a.type != GL_DOUBLE;
            glVertexAttribPointer(a.position, a.tupleSize, a.type, normalize, g->sizeOfVertex(), (void *) (qintptr) (offset + draw.vertices));
            offset += a.tupleSize * size_of_type(a.type);
        }
        if (m_useDepthBuffer)
            glVertexAttribPointer(sms->pos_order, 1, GL_FLOAT, false, 0, (void *) (qintptr) (draw.zorders));

        glDrawElements(g->drawingMode(), draw.indexCount, GL_UNSIGNED_SHORT, (void *) (qintptr) (indexBase + draw.indices));
    }
}

void Renderer::renderUnmergedBatch(const Batch *batch)
{
    if (batch->vertexCount == 0)
        return;

    Element *e = batch->first;
    Q_ASSERT(e);

    if (Q_UNLIKELY(debug_render())) {
        qDebug() << " -"
                 << batch
                 << (batch->uploadedThisFrame ? "[  upload]" : "[retained]")
                 << (e->node->clipList() ? "[  clip]" : "[noclip]")
                 << (batch->isOpaque ? "[opaque]" : "[ alpha]")
                 << "[unmerged]"
                 << " Nodes:" << QString::fromLatin1("%1").arg(qsg_countNodesInBatch(batch), 4).toLatin1().constData()
                 << " Vertices:" << QString::fromLatin1("%1").arg(batch->vertexCount, 5).toLatin1().constData()
                 << " Indices:" << QString::fromLatin1("%1").arg(batch->indexCount, 5).toLatin1().constData()
                 << " root:" << batch->root;

        batch->uploadedThisFrame = false;
    }

    QSGGeometryNode *gn = e->node;

    m_current_projection_matrix = projectionMatrix();
    updateClip(gn->clipList(), batch);

    glBindBuffer(GL_ARRAY_BUFFER, batch->vbo.id);
    char *indexBase = 0;
#ifdef QSG_SEPARATE_INDEX_BUFFER
    const Buffer *indexBuf = &batch->ibo;
#else
    const Buffer *indexBuf = &batch->vbo;
#endif
    if (batch->indexCount) {
        if (m_context->hasBrokenIndexBufferObjects()) {
            indexBase = indexBuf->data;
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        } else {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf->id);
        }
    }

    // We always have dirty matrix as all batches are at a unique z range.
    QSGMaterialShader::RenderState::DirtyStates dirty = QSGMaterialShader::RenderState::DirtyMatrix;

    QSGMaterial *material = gn->activeMaterial();
    ShaderManager::Shader *sms = m_shaderManager->prepareMaterialNoRewrite(material);
    if (!sms)
        return;
    QSGMaterialShader *program = sms->program;

    if (sms != m_currentShader)
        setActiveShader(program, sms);

    m_current_opacity = gn->inheritedOpacity();
    if (sms->lastOpacity != m_current_opacity) {
        dirty |= QSGMaterialShader::RenderState::DirtyOpacity;
        sms->lastOpacity = m_current_opacity;
    }

    int vOffset = 0;
#ifdef QSG_SEPARATE_INDEX_BUFFER
    char *iOffset = indexBase;
#else
    char *iOffset = indexBase + batch->vertexCount * gn->geometry()->sizeOfVertex();
#endif

    QMatrix4x4 rootMatrix = batch->root ? qsg_matrixForRoot(batch->root) : QMatrix4x4();

    while (e) {
        gn = e->node;

        m_current_model_view_matrix = rootMatrix * *gn->matrix();
        m_current_determinant = m_current_model_view_matrix.determinant();

        m_current_projection_matrix = projectionMatrix();
        if (m_useDepthBuffer) {
            m_current_projection_matrix(2, 2) = m_zRange;
            m_current_projection_matrix(2, 3) = 1.0f - e->order * m_zRange;
        }

        program->updateState(state(dirty), material, m_currentMaterial);

#ifndef QT_NO_DEBUG
    if (qsg_test_and_clear_material_failure()) {
        qDebug() << "QSGMaterial::updateState() triggered an error (unmerged), batch will be skipped:";
        qDebug() << "   - offending node is" << e->node;
        QSGNodeDumper::dump(rootNode());
        qFatal("Aborting: scene graph is invalid...");
        return;
    }
#endif

        // We don't need to bother with asking each node for its material as they
        // are all identical (compare==0) since they are in the same batch.
        m_currentMaterial = material;

        QSGGeometry* g = gn->geometry();
        char const *const *attrNames = program->attributeNames();
        int offset = 0;
        for (int j = 0; attrNames[j]; ++j) {
            if (!*attrNames[j])
                continue;
            const QSGGeometry::Attribute &a = g->attributes()[j];
            GLboolean normalize = a.type != GL_FLOAT && a.type != GL_DOUBLE;
            glVertexAttribPointer(a.position, a.tupleSize, a.type, normalize, g->sizeOfVertex(), (void *) (qintptr) (offset + vOffset));
            offset += a.tupleSize * size_of_type(a.type);
        }

        updateLineWidth(g);
        if (g->indexCount())
            glDrawElements(g->drawingMode(), g->indexCount(), g->indexType(), iOffset);
        else
            glDrawArrays(g->drawingMode(), 0, g->vertexCount());

        vOffset += g->sizeOfVertex() * g->vertexCount();
        iOffset += g->indexCount() * g->sizeOfIndex();

        // We only need to push this on the very first iteration...
        dirty &= ~QSGMaterialShader::RenderState::DirtyOpacity;

        e = e->nextInBatch;
    }
}

void Renderer::updateLineWidth(QSGGeometry *g)
{
    if (g->drawingMode() == GL_LINE_STRIP || g->drawingMode() == GL_LINE_LOOP || g->drawingMode() == GL_LINES)
        glLineWidth(g->lineWidth());
#if !defined(QT_OPENGL_ES_2)
    else if (!QOpenGLContext::currentContext()->isOpenGLES() && g->drawingMode() == GL_POINTS) {
        QOpenGLFunctions_1_0 *gl1funcs = 0;
        QOpenGLFunctions_3_2_Core *gl3funcs = 0;
        if (QOpenGLContext::currentContext()->format().profile() == QSurfaceFormat::CoreProfile)
            gl3funcs = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_3_2_Core>();
        else
            gl1funcs = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_1_0>();
        Q_ASSERT(gl1funcs || gl3funcs);
        if (gl1funcs)
            gl1funcs->glPointSize(g->lineWidth());
        else
            gl3funcs->glPointSize(g->lineWidth());
    }
#endif
}

void Renderer::renderBatches()
{
    if (Q_UNLIKELY(debug_render())) {
        qDebug().nospace() << "Rendering:" << endl
                           << " -> Opaque: " << qsg_countNodesInBatches(m_opaqueBatches) << " nodes in " << m_opaqueBatches.size() << " batches..." << endl
                           << " -> Alpha: " << qsg_countNodesInBatches(m_alphaBatches) << " nodes in " << m_alphaBatches.size() << " batches...";
    }

    QRect r = viewportRect();
    glViewport(r.x(), deviceRect().bottom() - r.bottom(), r.width(), r.height());
    glClearColor(clearColor().redF(), clearColor().greenF(), clearColor().blueF(), clearColor().alphaF());

    if (m_useDepthBuffer) {
        glClearDepthf(1); // calls glClearDepth() under the hood for desktop OpenGL
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(true);
        glDisable(GL_BLEND);
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(false);
    }
    glDisable(GL_CULL_FACE);
    glColorMask(true, true, true, true);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);

    bindable()->clear(clearMode());

    m_current_opacity = 1;
    m_currentMaterial = 0;
    m_currentShader = 0;
    m_currentProgram = 0;
    m_currentClip = 0;

    bool renderOpaque = !debug_noopaque();
    bool renderAlpha = !debug_noalpha();

    if (Q_LIKELY(renderOpaque)) {
        for (int i=0; i<m_opaqueBatches.size(); ++i) {
            Batch *b = m_opaqueBatches.at(i);
            if (b->merged)
                renderMergedBatch(b);
            else
                renderUnmergedBatch(b);
        }
    }

    glEnable(GL_BLEND);
    if (m_useDepthBuffer)
        glDepthMask(false);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    if (Q_LIKELY(renderAlpha)) {
        for (int i=0; i<m_alphaBatches.size(); ++i) {
            Batch *b = m_alphaBatches.at(i);
            if (b->merged)
                renderMergedBatch(b);
            else if (b->isRenderNode)
                renderRenderNode(b);
            else
                renderUnmergedBatch(b);
        }
    }

    if (m_currentShader)
        setActiveShader(0, 0);
    updateStencilClip(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDepthMask(true);
}

void Renderer::deleteRemovedElements()
{
    if (!m_elementsToDelete.size())
        return;

    for (int i=0; i<m_opaqueRenderList.size(); ++i) {
        Element **e = m_opaqueRenderList.data() + i;
        if (*e && (*e)->removed)
            *e = 0;
    }
    for (int i=0; i<m_alphaRenderList.size(); ++i) {
        Element **e = m_alphaRenderList.data() + i;
        if (*e && (*e)->removed)
            *e = 0;
    }

    for (int i=0; i<m_elementsToDelete.size(); ++i) {
        Element *e = m_elementsToDelete.at(i);
        if (e->isRenderNode)
            delete static_cast<RenderNodeElement *>(e);
        else
            m_elementAllocator.release(e);
    }
    m_elementsToDelete.reset();
}

void Renderer::render()
{
    if (Q_UNLIKELY(debug_dump())) {
        qDebug("\n");
        QSGNodeDumper::dump(rootNode());
    }

    QElapsedTimer timer;
    quint64 timeRenderLists = 0;
    quint64 timePrepareOpaque = 0;
    quint64 timePrepareAlpha = 0;
    quint64 timeSorting = 0;
    quint64 timeUploadOpaque = 0;
    quint64 timeUploadAlpha = 0;

    if (Q_UNLIKELY(debug_render() || debug_build())) {
        QByteArray type("rebuild:");
        if (m_rebuild == 0)
            type += " none";
        if (m_rebuild == FullRebuild)
            type += " full";
        else {
            if (m_rebuild & BuildRenderLists)
                type += " renderlists";
            else if (m_rebuild & BuildRenderListsForTaggedRoots)
                type += " partial";
            else if (m_rebuild & BuildBatches)
                type += " batches";
        }

        qDebug() << "Renderer::render()" << this << type;
        timer.start();
    }

    if (m_vao)
        m_vao->bind();

    if (m_rebuild & (BuildRenderLists | BuildRenderListsForTaggedRoots)) {
        bool complete = (m_rebuild & BuildRenderLists) != 0;
        if (complete)
            buildRenderListsFromScratch();
        else
            buildRenderListsForTaggedRoots();
        m_rebuild |= BuildBatches;

        if (Q_UNLIKELY(debug_build())) {
            qDebug() << "Opaque render lists" << (complete ? "(complete)" : "(partial)") << ":";
            for (int i=0; i<m_opaqueRenderList.size(); ++i) {
                Element *e = m_opaqueRenderList.at(i);
                qDebug() << " - element:" << e << " batch:" << e->batch << " node:" << e->node << " order:" << e->order;
            }
            qDebug() << "Alpha render list:" << (complete ? "(complete)" : "(partial)") << ":";
            for (int i=0; i<m_alphaRenderList.size(); ++i) {
                Element *e = m_alphaRenderList.at(i);
                qDebug() << " - element:" << e << " batch:" << e->batch << " node:" << e->node << " order:" << e->order;
            }
        }
    }
    if (Q_UNLIKELY(debug_render())) timeRenderLists = timer.restart();

    for (int i=0; i<m_opaqueBatches.size(); ++i)
        m_opaqueBatches.at(i)->cleanupRemovedElements();
    for (int i=0; i<m_alphaBatches.size(); ++i)
        m_alphaBatches.at(i)->cleanupRemovedElements();
    deleteRemovedElements();

    cleanupBatches(&m_opaqueBatches);
    cleanupBatches(&m_alphaBatches);

    if (m_rebuild & BuildBatches) {
        prepareOpaqueBatches();
        if (Q_UNLIKELY(debug_render())) timePrepareOpaque = timer.restart();
        prepareAlphaBatches();
        if (Q_UNLIKELY(debug_render())) timePrepareAlpha = timer.restart();

        if (Q_UNLIKELY(debug_build())) {
            qDebug() << "Opaque Batches:";
            for (int i=0; i<m_opaqueBatches.size(); ++i) {
                Batch *b = m_opaqueBatches.at(i);
                qDebug() << " - Batch " << i << b << (b->needsUpload ? "upload" : "") << " root:" << b->root;
                for (Element *e = b->first; e; e = e->nextInBatch) {
                    qDebug() << "   - element:" << e << " node:" << e->node << e->order;
                }
            }
            qDebug() << "Alpha Batches:";
            for (int i=0; i<m_alphaBatches.size(); ++i) {
                Batch *b = m_alphaBatches.at(i);
                qDebug() << " - Batch " << i << b << (b->needsUpload ? "upload" : "") << " root:" << b->root;
                for (Element *e = b->first; e; e = e->nextInBatch) {
                    qDebug() << "   - element:" << e << e->bounds << " node:" << e->node << " order:" << e->order;
                }
            }
        }
    } else {
        if (Q_UNLIKELY(debug_render())) timePrepareOpaque = timePrepareAlpha = timer.restart();
    }


    deleteRemovedElements();

    if (m_rebuild != 0) {
        // Then sort opaque batches so that we're drawing the batches with the highest
        // order first, maximizing the benefit of front-to-back z-ordering.
        if (m_opaqueBatches.size())
            std::sort(&m_opaqueBatches.first(), &m_opaqueBatches.last() + 1, qsg_sort_batch_decreasing_order);

        // Sort alpha batches back to front so that they render correctly.
        if (m_alphaBatches.size())
            std::sort(&m_alphaBatches.first(), &m_alphaBatches.last() + 1, qsg_sort_batch_increasing_order);

        m_zRange = m_nextRenderOrder != 0
                 ? 1.0 / (m_nextRenderOrder)
                 : 0;
    }

    if (Q_UNLIKELY(debug_render())) timeSorting = timer.restart();

    int largestVBO = 0;
#ifdef QSG_SEPARATE_INDEX_BUFFER
    int largestIBO = 0;
#endif

    if (Q_UNLIKELY(debug_upload())) qDebug() << "Uploading Opaque Batches:";
    for (int i=0; i<m_opaqueBatches.size(); ++i) {
        Batch *b = m_opaqueBatches.at(i);
        largestVBO = qMax(b->vbo.size, largestVBO);
#ifdef QSG_SEPARATE_INDEX_BUFFER
        largestIBO = qMax(b->ibo.size, largestIBO);
#endif
        uploadBatch(b);
    }
    if (Q_UNLIKELY(debug_render())) timeUploadOpaque = timer.restart();


    if (Q_UNLIKELY(debug_upload())) qDebug() << "Uploading Alpha Batches:";
    for (int i=0; i<m_alphaBatches.size(); ++i) {
        Batch *b = m_alphaBatches.at(i);
        uploadBatch(b);
        largestVBO = qMax(b->vbo.size, largestVBO);
#ifdef QSG_SEPARATE_INDEX_BUFFER
        largestIBO = qMax(b->ibo.size, largestIBO);
#endif
    }
    if (Q_UNLIKELY(debug_render())) timeUploadAlpha = timer.restart();

    if (largestVBO * 2 < m_vertexUploadPool.size())
        m_vertexUploadPool.resize(largestVBO * 2);
#ifdef QSG_SEPARATE_INDEX_BUFFER
    if (largestIBO * 2 < m_indexUploadPool.size())
        m_indexUploadPool.resize(largestIBO * 2);
#endif

    renderBatches();

    if (Q_UNLIKELY(debug_render())) {
        qDebug(" -> times: build: %d, prepare(opaque/alpha): %d/%d, sorting: %d, upload(opaque/alpha): %d/%d, render: %d",
               (int) timeRenderLists,
               (int) timePrepareOpaque, (int) timePrepareAlpha,
               (int) timeSorting,
               (int) timeUploadOpaque, (int) timeUploadAlpha,
               (int) timer.elapsed());
    }

    m_rebuild = 0;
    m_renderOrderRebuildLower = -1;
    m_renderOrderRebuildUpper = -1;

    if (m_visualizeMode != VisualizeNothing)
        visualize();

    if (m_vao)
        m_vao->release();
}

struct RenderNodeState : public QSGRenderNode::RenderState
{
    const QMatrix4x4 *projectionMatrix() const override { return m_projectionMatrix; }
    QRect scissorRect() const override { return m_scissorRect; }
    bool scissorEnabled() const override { return m_scissorEnabled; }
    int stencilValue() const override { return m_stencilValue; }
    bool stencilEnabled() const override { return m_stencilEnabled; }
    const QRegion *clipRegion() const override { return nullptr; }

    const QMatrix4x4 *m_projectionMatrix;
    QRect m_scissorRect;
    int m_stencilValue;
    bool m_scissorEnabled;
    bool m_stencilEnabled;
};

void Renderer::renderRenderNode(Batch *batch)
{
    if (Q_UNLIKELY(debug_render()))
        qDebug() << " -" << batch << "rendernode";

    Q_ASSERT(batch->first->isRenderNode);
    RenderNodeElement *e = (RenderNodeElement *) batch->first;

    setActiveShader(0, 0);

    QSGNode *clip = e->renderNode->parent();
    QSGRenderNodePrivate *rd = QSGRenderNodePrivate::get(e->renderNode);
    rd->m_clip_list = 0;
    while (clip != rootNode()) {
        if (clip->type() == QSGNode::ClipNodeType) {
            rd->m_clip_list = static_cast<QSGClipNode *>(clip);
            break;
        }
        clip = clip->parent();
    }

    updateClip(rd->m_clip_list, batch);

    RenderNodeState state;
    QMatrix4x4 pm = projectionMatrix();
    state.m_projectionMatrix = &pm;
    state.m_scissorEnabled = m_currentClipType & ScissorClip;
    state.m_stencilEnabled = m_currentClipType & StencilClip;
    state.m_scissorRect = m_currentScissorRect;
    state.m_stencilValue = m_currentStencilValue;

    QSGNode *xform = e->renderNode->parent();
    QMatrix4x4 matrix;
    QSGNode *root = rootNode();
    if (e->root) {
        matrix = qsg_matrixForRoot(e->root);
        root = e->root->sgNode;
    }
    while (xform != root) {
        if (xform->type() == QSGNode::TransformNodeType) {
            matrix = matrix * static_cast<QSGTransformNode *>(xform)->combinedMatrix();
            break;
        }
        xform = xform->parent();
    }
    rd->m_matrix = &matrix;

    QSGNode *opacity = e->renderNode->parent();
    rd->m_opacity = 1.0;
    while (opacity != rootNode()) {
        if (opacity->type() == QSGNode::OpacityNodeType) {
            rd->m_opacity = static_cast<QSGOpacityNode *>(opacity)->combinedOpacity();
            break;
        }
        opacity = opacity->parent();
    }

    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    QSGRenderNode::StateFlags changes = e->renderNode->changedStates();

    GLuint prevFbo = 0;
    if (changes & QSGRenderNode::RenderTargetState)
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *) &prevFbo);

    e->renderNode->render(&state);

    rd->m_matrix = 0;
    rd->m_clip_list = 0;

    if (changes & QSGRenderNode::ViewportState) {
        QRect r = viewportRect();
        glViewport(r.x(), deviceRect().bottom() - r.bottom(), r.width(), r.height());
    }

    if (changes & QSGRenderNode::StencilState) {
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilMask(0xff);
        glDisable(GL_STENCIL_TEST);
    }

    if (changes & (QSGRenderNode::StencilState | QSGRenderNode::ScissorState)) {
        glDisable(GL_SCISSOR_TEST);
        m_currentClip = 0;
        m_currentClipType = NoClip;
    }

    if (changes & QSGRenderNode::DepthState)
        glDisable(GL_DEPTH_TEST);

    if (changes & QSGRenderNode::ColorState)
        bindable()->reactivate();

    if (changes & QSGRenderNode::BlendState) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }

    if (changes & QSGRenderNode::CullState) {
        glFrontFace(isMirrored() ? GL_CW : GL_CCW);
        glDisable(GL_CULL_FACE);
    }

    if (changes & QSGRenderNode::RenderTargetState)
        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
}

class VisualizeShader : public QOpenGLShaderProgram
{
public:
    int color;
    int matrix;
    int rotation;
    int pattern;
    int projection;
};

void Renderer::visualizeDrawGeometry(const QSGGeometry *g)
{
    if (g->attributeCount() < 1)
        return;
    const QSGGeometry::Attribute *a = g->attributes();
    glVertexAttribPointer(0, a->tupleSize, a->type, false, g->sizeOfVertex(), g->vertexData());
    if (g->indexCount())
        glDrawElements(g->drawingMode(), g->indexCount(), g->indexType(), g->indexData());
    else
        glDrawArrays(g->drawingMode(), 0, g->vertexCount());

}

void Renderer::visualizeBatch(Batch *b)
{
    VisualizeShader *shader = static_cast<VisualizeShader *>(m_shaderManager->visualizeProgram);

    if (b->positionAttribute != 0)
        return;

    QSGGeometryNode *gn = b->first->node;
    QSGGeometry *g = gn->geometry();
    const QSGGeometry::Attribute &a = g->attributes()[b->positionAttribute];

    glBindBuffer(GL_ARRAY_BUFFER, b->vbo.id);

    QMatrix4x4 matrix(m_current_projection_matrix);
    if (b->root)
        matrix = matrix * qsg_matrixForRoot(b->root);

    shader->setUniformValue(shader->pattern, float(b->merged ? 0 : 1));

    QColor color = QColor::fromHsvF((rand() & 1023) / 1023.0, 1.0, 1.0);
    float cr = color.redF();
    float cg = color.greenF();
    float cb = color.blueF();
    shader->setUniformValue(shader->color, cr, cg, cb, 1.0);

    if (b->merged) {
        shader->setUniformValue(shader->matrix, matrix);
        for (int ds=0; ds<b->drawSets.size(); ++ds) {
            const DrawSet &set = b->drawSets.at(ds);
            glVertexAttribPointer(a.position, 2, a.type, false, g->sizeOfVertex(), (void *) (qintptr) (set.vertices));
            glDrawElements(g->drawingMode(), set.indexCount, GL_UNSIGNED_SHORT, (void *) (qintptr) (b->vbo.data + set.indices));
        }
    } else {
        Element *e = b->first;
        int offset = 0;
        while (e) {
            gn = e->node;
            g = gn->geometry();
            shader->setUniformValue(shader->matrix, matrix * *gn->matrix());
            glVertexAttribPointer(a.position, a.tupleSize, a.type, false, g->sizeOfVertex(), (void *) (qintptr) offset);
            if (g->indexCount())
                glDrawElements(g->drawingMode(), g->indexCount(), g->indexType(), g->indexData());
            else
                glDrawArrays(g->drawingMode(), 0, g->vertexCount());
            offset += g->sizeOfVertex() * g->vertexCount();
            e = e->nextInBatch;
        }
    }
}




void Renderer::visualizeClipping(QSGNode *node)
{
    if (node->type() == QSGNode::ClipNodeType) {
        VisualizeShader *shader = static_cast<VisualizeShader *>(m_shaderManager->visualizeProgram);
        QSGClipNode *clipNode = static_cast<QSGClipNode *>(node);
        QMatrix4x4 matrix = m_current_projection_matrix;
        if (clipNode->matrix())
            matrix = matrix * *clipNode->matrix();
        shader->setUniformValue(shader->matrix, matrix);
        visualizeDrawGeometry(clipNode->geometry());
    }

    QSGNODE_TRAVERSE(node) {
        visualizeClipping(child);
    }
}

#define QSGNODE_DIRTY_PARENT (QSGNode::DirtyNodeAdded \
                              | QSGNode::DirtyOpacity \
                              | QSGNode::DirtyMatrix \
                              | QSGNode::DirtyNodeRemoved)

void Renderer::visualizeChangesPrepare(Node *n, uint parentChanges)
{
    uint childDirty = (parentChanges | n->dirtyState) & QSGNODE_DIRTY_PARENT;
    uint selfDirty = n->dirtyState | parentChanges;
    if (n->type() == QSGNode::GeometryNodeType && selfDirty != 0)
        m_visualizeChanceSet.insert(n, selfDirty);
    SHADOWNODE_TRAVERSE(n) {
        visualizeChangesPrepare(child, childDirty);
    }
}

void Renderer::visualizeChanges(Node *n)
{

    if (n->type() == QSGNode::GeometryNodeType && n->element()->batch && m_visualizeChanceSet.contains(n)) {
        uint dirty = m_visualizeChanceSet.value(n);
        bool tinted = (dirty & QSGNODE_DIRTY_PARENT) != 0;

        VisualizeShader *shader = static_cast<VisualizeShader *>(m_shaderManager->visualizeProgram);
        QColor color = QColor::fromHsvF((rand() & 1023) / 1023.0, 0.3, 1.0);
        float ca = 0.5;
        float cr = color.redF() * ca;
        float cg = color.greenF() * ca;
        float cb = color.blueF() * ca;
        shader->setUniformValue(shader->color, cr, cg, cb, ca);
        shader->setUniformValue(shader->pattern, float(tinted ? 0.5 : 0));

        QSGGeometryNode *gn = static_cast<QSGGeometryNode *>(n->sgNode);

        QMatrix4x4 matrix = m_current_projection_matrix;
        if (n->element()->batch->root)
            matrix = matrix * qsg_matrixForRoot(n->element()->batch->root);
        matrix = matrix * *gn->matrix();
        shader->setUniformValue(shader->matrix, matrix);
        visualizeDrawGeometry(gn->geometry());

        // This is because many changes don't propegate their dirty state to the
        // parent so the node updater will not unset these states. They are
        // not used for anything so, unsetting it should have no side effects.
        n->dirtyState = 0;
    }

    SHADOWNODE_TRAVERSE(n) {
        visualizeChanges(child);
    }
}

void Renderer::visualizeOverdraw_helper(Node *node)
{
    if (node->type() == QSGNode::GeometryNodeType && node->element()->batch) {
        VisualizeShader *shader = static_cast<VisualizeShader *>(m_shaderManager->visualizeProgram);
        QSGGeometryNode *gn = static_cast<QSGGeometryNode *>(node->sgNode);

        QMatrix4x4 matrix = m_current_projection_matrix;
        matrix(2, 2) = m_zRange;
        matrix(2, 3) = 1.0f - node->element()->order * m_zRange;

        if (node->element()->batch->root)
            matrix = matrix * qsg_matrixForRoot(node->element()->batch->root);
        matrix = matrix * *gn->matrix();
        shader->setUniformValue(shader->matrix, matrix);

        QColor color = node->element()->batch->isOpaque ? QColor::fromRgbF(0.3, 1.0, 0.3) : QColor::fromRgbF(1.0, 0.3, 0.3);
        float ca = 0.33f;
        shader->setUniformValue(shader->color, color.redF() * ca, color.greenF() * ca, color.blueF() * ca, ca);

        visualizeDrawGeometry(gn->geometry());
    }

    SHADOWNODE_TRAVERSE(node) {
        visualizeOverdraw_helper(child);
    }
}

void Renderer::visualizeOverdraw()
{
    VisualizeShader *shader = static_cast<VisualizeShader *>(m_shaderManager->visualizeProgram);
    shader->setUniformValue(shader->color, 0.5f, 0.5f, 1.0f, 1.0f);
    shader->setUniformValue(shader->projection, 1);

    glBlendFunc(GL_ONE, GL_ONE);

    static float step = 0;
    step += static_cast<float>(M_PI * 2 / 1000.);
    if (step > M_PI * 2)
        step = 0;
    float angle = 80.0 * std::sin(step);

    QMatrix4x4 xrot; xrot.rotate(20, 1, 0, 0);
    QMatrix4x4 zrot; zrot.rotate(angle, 0, 0, 1);
    QMatrix4x4 tx; tx.translate(0, 0, 1);

    QMatrix4x4 m;

//    m.rotate(180, 0, 1, 0);

    m.translate(0, 0.5, 4);
    m.scale(2, 2, 1);

    m.rotate(-30, 1, 0, 0);
    m.rotate(angle, 0, 1, 0);
    m.translate(0, 0, -1);

    shader->setUniformValue(shader->rotation, m);

    float box[] = {
        // lower
        -1, 1, 0,   1, 1, 0,
        -1, 1, 0,   -1, -1, 0,
        1, 1, 0,    1, -1, 0,
        -1, -1, 0,  1, -1, 0,

        // upper
        -1, 1, 1,   1, 1, 1,
        -1, 1, 1,   -1, -1, 1,
        1, 1, 1,    1, -1, 1,
        -1, -1, 1,  1, -1, 1,

        // sides
        -1, -1, 0,  -1, -1, 1,
        1, -1, 0,   1, -1, 1,
        -1, 1, 0,   -1, 1, 1,
        1, 1, 0,    1, 1, 1
    };
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, box);
    glLineWidth(2);
    glDrawArrays(GL_LINES, 0, 24);

    visualizeOverdraw_helper(m_nodes.value(rootNode()));

    // Animate the view...
    QSurface *surface = QOpenGLContext::currentContext()->surface();
    if (surface->surfaceClass() == QSurface::Window)
        if (QQuickWindow *window = qobject_cast<QQuickWindow *>(static_cast<QWindow *>(surface)))
            window->update();
}

void Renderer::setCustomRenderMode(const QByteArray &mode)
{
    if (mode.isEmpty()) m_visualizeMode = VisualizeNothing;
    else if (mode == "clip") m_visualizeMode = VisualizeClipping;
    else if (mode == "overdraw") m_visualizeMode = VisualizeOverdraw;
    else if (mode == "batches") m_visualizeMode = VisualizeBatches;
    else if (mode == "changes") m_visualizeMode = VisualizeChanges;
}

void Renderer::visualize()
{
    if (!m_shaderManager->visualizeProgram) {
        VisualizeShader *prog = new VisualizeShader();
        QSGShaderSourceBuilder::initializeProgramFromFiles(
            prog,
            QStringLiteral(":/qt-project.org/scenegraph/shaders/visualization.vert"),
            QStringLiteral(":/qt-project.org/scenegraph/shaders/visualization.frag"));
        prog->bindAttributeLocation("v", 0);
        prog->link();
        prog->bind();
        prog->color = prog->uniformLocation("color");
        prog->pattern = prog->uniformLocation("pattern");
        prog->projection = prog->uniformLocation("projection");
        prog->matrix = prog->uniformLocation("matrix");
        prog->rotation = prog->uniformLocation("rotation");
        m_shaderManager->visualizeProgram = prog;
    } else {
        m_shaderManager->visualizeProgram->bind();
    }
    VisualizeShader *shader = static_cast<VisualizeShader *>(m_shaderManager->visualizeProgram);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnableVertexAttribArray(0);

    // Blacken out the actual rendered content...
    float bgOpacity = 0.8f;
    if (m_visualizeMode == VisualizeBatches)
        bgOpacity = 1.0;
    float v[] = { -1, 1,   1, 1,   -1, -1,   1, -1 };
    shader->setUniformValue(shader->color, 0.0f, 0.0f, 0.0f, bgOpacity);
    shader->setUniformValue(shader->matrix, QMatrix4x4());
    shader->setUniformValue(shader->rotation, QMatrix4x4());
    shader->setUniformValue(shader->pattern, 0.0f);
    shader->setUniformValue(shader->projection, false);
    glVertexAttribPointer(0, 2, GL_FLOAT, false, 0, v);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (m_visualizeMode == VisualizeBatches) {
        srand(0); // To force random colors to be roughly the same every time..
        for (int i=0; i<m_opaqueBatches.size(); ++i) visualizeBatch(m_opaqueBatches.at(i));
        for (int i=0; i<m_alphaBatches.size(); ++i) visualizeBatch(m_alphaBatches.at(i));
    } else if (m_visualizeMode == VisualizeClipping) {
        shader->setUniformValue(shader->pattern, 0.5f);
        shader->setUniformValue(shader->color, 0.2f, 0.0f, 0.0f, 0.2f);
        visualizeClipping(rootNode());
    } else if (m_visualizeMode == VisualizeChanges) {
        visualizeChanges(m_nodes.value(rootNode()));
        m_visualizeChanceSet.clear();
    } else if (m_visualizeMode == VisualizeOverdraw) {
        visualizeOverdraw();
    }

    // Reset state back to defaults..
    glDisable(GL_BLEND);
    glDisableVertexAttribArray(0);
    shader->release();
}

QT_END_NAMESPACE

}
