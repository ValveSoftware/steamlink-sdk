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

#include "qsgnode.h"
#include "qsgnode_p.h"
#include "qsgrenderer_p.h"
#include "qsgnodeupdater_p.h"
#include "qsgmaterial.h"

#include "limits.h"

QT_BEGIN_NAMESPACE

#ifndef QT_NO_DEBUG
static const bool qsg_leak_check = !qEnvironmentVariableIsEmpty("QML_LEAK_CHECK");
static int qt_node_count = 0;

static void qt_print_node_count()
{
    qDebug("Number of leaked nodes: %i", qt_node_count);
    qt_node_count = -1;
}
#endif

/*!
    \group qtquick-scenegraph-nodes
    \title Qt Quick Scene Graph Node classes
    \brief Nodes that can be used as part of the scene graph.

    This page lists the nodes in \l {Qt Quick}'s \l {scene graph}{Qt Quick Scene Graph}.
 */

/*!
    \class QSGNode
    \brief The QSGNode class is the base class for all nodes in the scene graph.

    \inmodule QtQuick
    \ingroup qtquick-scenegraph-nodes

    The QSGNode class can be used as a child container. Children are added with
    the appendChildNode(), prependChildNode(), insertChildNodeBefore() and
    insertChildNodeAfter(). The order of nodes is important as geometry nodes
    are rendered according to their ordering in the scene graph.

    The scene graph nodes contains a mechanism to describe which
    parts of the scene has changed. This includes the combined matrices,
    accumulated opacity, changes to the node hierarchy, and so on. This
    information can be used for optimizations inside the scene graph renderer.
    For the renderer to properly render the nodes, it is important that users
    call QSGNode::markDirty() with the correct flags when nodes are changed.
    Most of the functions on the node classes will implicitly call markDirty().
    For example, QSGNode::appendChildNode() will call markDirty() passing in
    QSGNode::DirtyNodeAdded.

    If nodes change every frame, the preprocess() function can be used to
    apply changes to a node for every frame it is rendered. The use of
    preprocess() must be explicitly enabled by setting the
    QSGNode::UsePreprocess flag on the node.

    The virtual isSubtreeBlocked() function can be used to disable a subtree all
    together. Nodes in a blocked subtree will not be preprocessed() and not
    rendered.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.
 */

/*!
    \enum QSGNode::DirtyStateBit

    Used in QSGNode::markDirty() to indicate how the scene graph has changed.

    \value DirtyMatrix The matrix in a QSGTransformNode has changed.
    \value DirtyNodeAdded A node was added.
    \value DirtyNodeRemoved A node was removed.
    \value DirtyGeometry The geometry of a QSGGeometryNode has changed.
    \value DirtyMaterial The material of a QSGGeometryNode has changed.
    \value DirtyOpacity The opacity of a QSGOpacityNode has changed.

    \sa QSGNode::markDirty()
 */

/*!
    \enum QSGNode::Flag

    The QSGNode::Flag enum describes flags on the QSGNode

    \value OwnedByParent The node is owned by its parent and will be deleted
    when the parent is deleted.
    \value UsePreprocess The node's virtual preprocess() function will be called
    before rendering starts.
    \value OwnsGeometry Only valid for QSGGeometryNode and QSGClipNode.
    The node has ownership over the QSGGeometry instance and will
    delete it when the node is destroyed or a geometry is assigned.
    \value OwnsMaterial Only valid for QSGGeometryNode. The node has ownership
    over the material and will delete it when the node is destroyed or a material is assigned.
    \value OwnsOpaqueMaterial Only valid for QSGGeometryNode. The node has
    ownership over the opaque material and will delete it when the node is
    destroyed or a material is assigned.
    \value InternalReserved Reserved for internal use.
 */

/*!
    \enum QSGNode::NodeType

    Can be used to figure out the type of node.

    \value BasicNodeType The type of QSGNode
    \value GeometryNodeType The type of QSGGeometryNode
    \value TransformNodeType The type of QSGTransformNode
    \value ClipNodeType The type of QSGClipNode
    \value OpacityNodeType The type of QSGOpacityNode

    \sa type()
 */

/*!
    \fn QSGNode *QSGNode::childAtIndex(int i) const

    Returns the child at index \a i.

    Children are stored internally as a linked list, so iterating
    over the children via the index is suboptimal.
 */

/*!
    \fn int QSGNode::childCount() const

    Returns the number of child nodes.
 */

/*!
    \fn void QSGNode::clearDirty()

    \internal
 */

/*!
    \fn QSGNode *QSGNode::firstChild() const

    Returns the first child of this node.

    The children are stored in a linked list.
 */

/*!
    \fn QSGNode *QSGNode::lastChild() const

    Returns the last child of this node.

    The children are stored as a linked list.
 */

/*!
    \fn QSGNode::Flags QSGNode::flags() const

    Returns the set of flags for this node.
 */

/*!
    \fn QSGNode *QSGNode::nextSibling() const

    Returns the node after this in the parent's list of children.

    The children are stored as a linked list.
 */

/*!
    \fn QSGNode *QSGNode::previousSibling() const

    Returns the node before this in the parent's list of children.

    The children are stored as a linked list.
 */

/*!
    \fn QSGNode::Type QSGNode::type() const

    Returns the type of this node. The node type must be one of the
    predefined types defined in QSGNode::NodeType and can safely be
    used to cast to the corresponding class.
 */

/*!
    \fn QSGNode::DirtyState QSGNode::dirtyState() const

    \internal
 */

/*!
    \fn QSGNode *QSGNode::parent() const

    Returns the parent node of this node.
 */


/*!
 * Constructs a new node
 */
QSGNode::QSGNode()
    : m_parent(0)
    , m_type(BasicNodeType)
    , m_firstChild(0)
    , m_lastChild(0)
    , m_nextSibling(0)
    , m_previousSibling(0)
    , m_subtreeRenderableCount(0)
    , m_nodeFlags(OwnedByParent)
    , m_dirtyState(0)
{
    init();
}

/*!
 * Constructs a new node with the given node type.
 *
 * \internal
 */
QSGNode::QSGNode(NodeType type)
    : m_parent(0)
    , m_type(type)
    , m_firstChild(0)
    , m_lastChild(0)
    , m_nextSibling(0)
    , m_previousSibling(0)
    , m_subtreeRenderableCount(type == GeometryNodeType || type == RenderNodeType ? 1 : 0)
    , m_nodeFlags(OwnedByParent)
    , m_dirtyState(0)
{
    init();
}

/*!
 * Constructs a new node with the given node type.
 *
 * \internal
 */
QSGNode::QSGNode(QSGNodePrivate &dd, NodeType type)
    : m_parent(0)
    , m_type(type)
    , m_firstChild(0)
    , m_lastChild(0)
    , m_nextSibling(0)
    , m_previousSibling(0)
    , m_subtreeRenderableCount(type == GeometryNodeType || type == RenderNodeType ? 1 : 0)
    , m_nodeFlags(OwnedByParent)
    , m_dirtyState(0)
    , d_ptr(&dd)
{
    init();
}

/*!
 * \internal
 */
void QSGNode::init()
{
#ifndef QT_NO_DEBUG
    if (qsg_leak_check) {
        ++qt_node_count;
        static bool atexit_registered = false;
        if (!atexit_registered) {
            atexit(qt_print_node_count);
            atexit_registered = true;
        }
    }
#endif

#ifdef QSG_RUNTIME_DESCRIPTION
    if (d_ptr.isNull())
        d_ptr.reset(new QSGNodePrivate());
#endif
}

/*!
 * Destroys the node.
 *
 * Every child of this node that has the flag QSGNode::OwnedByParent set,
 * will also be deleted.
 */
QSGNode::~QSGNode()
{
#ifndef QT_NO_DEBUG
    if (qsg_leak_check) {
        --qt_node_count;
        if (qt_node_count < 0)
            qDebug("Node destroyed after qt_print_node_count() was called.");
    }
#endif
    destroy();
}


/*!
    \fn void QSGNode::preprocess()

    Override this function to do processing on the node before it is rendered.

    Preprocessing needs to be explicitly enabled by setting the flag
    QSGNode::UsePreprocess. The flag needs to be set before the node is added
    to the scene graph and will cause the preprocess() function to be called
    for every frame the node is rendered.

    The preprocess function is called before the update pass that propagates
    opacity and transformations through the scene graph. That means that
    functions like QSGOpacityNode::combinedOpacity() and
    QSGTransformNode::combinedMatrix() will not contain up-to-date values.
    If such values are changed during the preprocess, these changes will be
    propagated through the scene graph before it is rendered.

    \warning Beware of deleting nodes while they are being preprocessed. It is
    possible, with a small performance hit, to delete a single node during its
    own preprocess call. Deleting a subtree which has nodes that also use
    preprocessing may result in a segmentation fault. This is done for
    performance reasons.
 */




/*!
    Returns whether this node and its subtree is available for use.

    Blocked subtrees will not get their dirty states updated and they
    will not be rendered.

    The QSGOpacityNode will return a blocked subtree when accumulated opacity
    is 0, for instance.
 */

bool QSGNode::isSubtreeBlocked() const
{
    return false;
}

/*!
    \internal
    Detaches the node from the scene graph and deletes any children it owns.

    This function is called from QSGNode's and QSGRootNode's destructor. It
    should not be called explicitly in user code. QSGRootNode needs to call
    destroy() because destroy() calls removeChildNode() which in turn calls
    markDirty() which type-casts the node to QSGRootNode. This type-cast is not
    valid at the time QSGNode's destructor is called because the node will
    already be partially destroyed at that point.
*/

void QSGNode::destroy()
{
    if (m_parent) {
        m_parent->removeChildNode(this);
        Q_ASSERT(m_parent == 0);
    }
    while (m_firstChild) {
        QSGNode *child = m_firstChild;
        removeChildNode(child);
        Q_ASSERT(child->m_parent == 0);
        if (child->flags() & OwnedByParent)
            delete child;
    }

    Q_ASSERT(m_firstChild == 0 && m_lastChild == 0);
}


/*!
    Prepends \a node to this node's the list of children.

    Ordering of nodes is important as geometry nodes will be rendered in the
    order they are added to the scene graph.
 */

void QSGNode::prependChildNode(QSGNode *node)
{
    //Q_ASSERT_X(!m_children.contains(node), "QSGNode::prependChildNode", "QSGNode is already a child!");
    Q_ASSERT_X(!node->m_parent, "QSGNode::prependChildNode", "QSGNode already has a parent");

#ifndef QT_NO_DEBUG
    if (node->type() == QSGNode::GeometryNodeType) {
        QSGGeometryNode *g = static_cast<QSGGeometryNode *>(node);
        Q_ASSERT_X(g->material(), "QSGNode::prependChildNode", "QSGGeometryNode is missing material");
        Q_ASSERT_X(g->geometry(), "QSGNode::prependChildNode", "QSGGeometryNode is missing geometry");
    }
#endif

    if (m_firstChild)
        m_firstChild->m_previousSibling = node;
    else
        m_lastChild = node;
    node->m_nextSibling = m_firstChild;
    m_firstChild = node;
    node->m_parent = this;

    node->markDirty(DirtyNodeAdded);
}

/*!
    Appends \a node to this node's list of children.

    Ordering of nodes is important as geometry nodes will be rendered in the
    order they are added to the scene graph.
 */

void QSGNode::appendChildNode(QSGNode *node)
{
    //Q_ASSERT_X(!m_children.contains(node), "QSGNode::appendChildNode", "QSGNode is already a child!");
    Q_ASSERT_X(!node->m_parent, "QSGNode::appendChildNode", "QSGNode already has a parent");

#ifndef QT_NO_DEBUG
    if (node->type() == QSGNode::GeometryNodeType) {
        QSGGeometryNode *g = static_cast<QSGGeometryNode *>(node);
        Q_ASSERT_X(g->material(), "QSGNode::appendChildNode", "QSGGeometryNode is missing material");
        Q_ASSERT_X(g->geometry(), "QSGNode::appendChildNode", "QSGGeometryNode is missing geometry");
    }
#endif

    if (m_lastChild)
        m_lastChild->m_nextSibling = node;
    else
        m_firstChild = node;
    node->m_previousSibling = m_lastChild;
    m_lastChild = node;
    node->m_parent = this;

    node->markDirty(DirtyNodeAdded);
}



/*!
    Inserts \a node to this node's list of children before the node specified with \a before.

    Ordering of nodes is important as geometry nodes will be rendered in the
    order they are added to the scene graph.
 */

void QSGNode::insertChildNodeBefore(QSGNode *node, QSGNode *before)
{
    //Q_ASSERT_X(!m_children.contains(node), "QSGNode::insertChildNodeBefore", "QSGNode is already a child!");
    Q_ASSERT_X(!node->m_parent, "QSGNode::insertChildNodeBefore", "QSGNode already has a parent");
    Q_ASSERT_X(before && before->m_parent == this, "QSGNode::insertChildNodeBefore", "The parent of \'before\' is wrong");

#ifndef QT_NO_DEBUG
    if (node->type() == QSGNode::GeometryNodeType) {
        QSGGeometryNode *g = static_cast<QSGGeometryNode *>(node);
        Q_ASSERT_X(g->material(), "QSGNode::insertChildNodeBefore", "QSGGeometryNode is missing material");
        Q_ASSERT_X(g->geometry(), "QSGNode::insertChildNodeBefore", "QSGGeometryNode is missing geometry");
    }
#endif

    QSGNode *previous = before->m_previousSibling;
    if (previous)
        previous->m_nextSibling = node;
    else
        m_firstChild = node;
    node->m_previousSibling = previous;
    node->m_nextSibling = before;
    before->m_previousSibling = node;
    node->m_parent = this;

    node->markDirty(DirtyNodeAdded);
}



/*!
    Inserts \a node to this node's list of children after the node specified with \a after.

    Ordering of nodes is important as geometry nodes will be rendered in the
    order they are added to the scene graph.
 */

void QSGNode::insertChildNodeAfter(QSGNode *node, QSGNode *after)
{
    //Q_ASSERT_X(!m_children.contains(node), "QSGNode::insertChildNodeAfter", "QSGNode is already a child!");
    Q_ASSERT_X(!node->m_parent, "QSGNode::insertChildNodeAfter", "QSGNode already has a parent");
    Q_ASSERT_X(after && after->m_parent == this, "QSGNode::insertChildNodeAfter", "The parent of \'after\' is wrong");

#ifndef QT_NO_DEBUG
    if (node->type() == QSGNode::GeometryNodeType) {
        QSGGeometryNode *g = static_cast<QSGGeometryNode *>(node);
        Q_ASSERT_X(g->material(), "QSGNode::insertChildNodeAfter", "QSGGeometryNode is missing material");
        Q_ASSERT_X(g->geometry(), "QSGNode::insertChildNodeAfter", "QSGGeometryNode is missing geometry");
    }
#endif

    QSGNode *next = after->m_nextSibling;
    if (next)
        next->m_previousSibling = node;
    else
        m_lastChild = node;
    node->m_nextSibling = next;
    node->m_previousSibling = after;
    after->m_nextSibling = node;
    node->m_parent = this;

    node->markDirty(DirtyNodeAdded);
}



/*!
    Removes \a node from this node's list of children.
 */

void QSGNode::removeChildNode(QSGNode *node)
{
    //Q_ASSERT(m_children.contains(node));
    Q_ASSERT(node->parent() == this);

    QSGNode *previous = node->m_previousSibling;
    QSGNode *next = node->m_nextSibling;
    if (previous)
        previous->m_nextSibling = next;
    else
        m_firstChild = next;
    if (next)
        next->m_previousSibling = previous;
    else
        m_lastChild = previous;
    node->m_previousSibling = 0;
    node->m_nextSibling = 0;

    node->markDirty(DirtyNodeRemoved);
    node->m_parent = 0;
}


/*!
    Removes all child nodes from this node's list of children.
 */

void QSGNode::removeAllChildNodes()
{
    while (m_firstChild) {
        QSGNode *node = m_firstChild;
        m_firstChild = node->m_nextSibling;
        node->m_nextSibling = 0;
        if (m_firstChild)
            m_firstChild->m_previousSibling = 0;
        else
            m_lastChild = 0;
        node->markDirty(DirtyNodeRemoved);
        node->m_parent = 0;
    }
}

/*!
 * \internal
 *
 * Reparents all nodes of this node to \a newParent.
 */
void QSGNode::reparentChildNodesTo(QSGNode *newParent)
{
    for (QSGNode *c = firstChild(); c; c = firstChild()) {
        removeChildNode(c);
        newParent->appendChildNode(c);
    }
}


int QSGNode::childCount() const
{
    int count = 0;
    QSGNode *n = m_firstChild;
    while (n) {
        ++count;
        n = n->m_nextSibling;
    }
    return count;
}


QSGNode *QSGNode::childAtIndex(int i) const
{
    QSGNode *n = m_firstChild;
    while (i && n) {
        --i;
        n = n->m_nextSibling;
    }
    return n;
}


/*!
    Sets the flag \a f on this node if \a enabled is true;
    otherwise clears the flag.

    \sa flags()
*/

void QSGNode::setFlag(Flag f, bool enabled)
{
    if (bool(m_nodeFlags & f) == enabled)
        return;
    m_nodeFlags ^= f;
    Q_ASSERT(int(UsePreprocess) == int(DirtyUsePreprocess));
    int changedFlag = f & UsePreprocess;
    if (changedFlag)
        markDirty(DirtyState(changedFlag));
}


/*!
    Sets the flags \a f on this node if \a enabled is true;
    otherwise clears the flags.

    \sa flags()
*/

void QSGNode::setFlags(Flags f, bool enabled)
{
    Flags oldFlags = m_nodeFlags;
    if (enabled)
        m_nodeFlags |= f;
    else
        m_nodeFlags &= ~f;
    Q_ASSERT(int(UsePreprocess) == int(DirtyUsePreprocess));
    int changedFlags = (oldFlags ^ m_nodeFlags) & UsePreprocess;
    if (changedFlags)
        markDirty(DirtyState(changedFlags));
}



/*!
    Notifies all connected renderers that the node has dirty \a bits.
 */

void QSGNode::markDirty(DirtyState bits)
{
    int renderableCountDiff = 0;
    if (bits & DirtyNodeAdded)
        renderableCountDiff += m_subtreeRenderableCount;
    if (bits & DirtyNodeRemoved)
        renderableCountDiff -= m_subtreeRenderableCount;

    QSGNode *p = m_parent;
    while (p) {
        p->m_subtreeRenderableCount += renderableCountDiff;
        if (p->type() == RootNodeType)
            static_cast<QSGRootNode *>(p)->notifyNodeChange(this, bits);
        p = p->m_parent;
    }
}

void qsgnode_set_description(QSGNode *node, const QString &description)
{
#ifdef QSG_RUNTIME_DESCRIPTION
    QSGNodePrivate::setDescription(node, description);
#else
    Q_UNUSED(node);
    Q_UNUSED(description);
#endif
}

/*!
    \class QSGBasicGeometryNode
    \brief The QSGBasicGeometryNode class serves as a baseclass for geometry based nodes.

    \inmodule QtQuick

    The QSGBasicGeometryNode class should not be used by itself. It is only encapsulates
    shared functionality between the QSGGeometryNode and QSGClipNode classes.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.
  */


/*!
    Creates a new basic geometry node of type \a type

    \internal
 */
QSGBasicGeometryNode::QSGBasicGeometryNode(NodeType type)
    : QSGNode(type)
    , m_geometry(0)
    , m_matrix(0)
    , m_clip_list(0)
{
}


/*!
    \internal
 */
QSGBasicGeometryNode::QSGBasicGeometryNode(QSGBasicGeometryNodePrivate &dd, NodeType type)
    : QSGNode(dd, type)
    , m_geometry(0)
    , m_matrix(0)
    , m_clip_list(0)
{
}


/*!
    Deletes this QSGBasicGeometryNode.

    If the node has the flag QSGNode::OwnsGeometry set, it will also delete the
    geometry object it is pointing to. This flag is not set by default.
  */

QSGBasicGeometryNode::~QSGBasicGeometryNode()
{
    if (flags() & OwnsGeometry)
        delete m_geometry;
}


/*!
    \fn QSGGeometry *QSGBasicGeometryNode::geometry()

    Returns this node's geometry.

    The geometry is null by default.
 */

/*!
    \fn const QSGGeometry *QSGBasicGeometryNode::geometry() const

    Returns this node's geometry.

    The geometry is null by default.
 */

/*!
    \fn QMatrix4x4 *QSGBasicGeometryNode::matrix() const

    Will be set during rendering to contain transformation of the geometry
    for that rendering pass.

    \internal
 */

/*!
    \fn QSGClipNode *QSGBasicGeometryNode::clipList() const

    Will be set during rendering to contain the clip of the geometry
    for that rendering pass.

    \internal
 */

/*!
    \fn void QSGBasicGeometryNode::setRendererMatrix(const QMatrix4x4 *m)

    \internal
 */

/*!
    \fn void QSGBasicGeometryNode::setRendererClipList(const QSGClipNode *c)

    \internal
 */


/*!
    Sets the geometry of this node to \a geometry.

    If the node has the flag QSGNode::OwnsGeometry set, it will also delete the
    geometry object it is pointing to. This flag is not set by default.

    If the geometry is changed whitout calling setGeometry() again, the user
    must also mark the geometry as dirty using QSGNode::markDirty().

    \sa markDirty()
 */

void QSGBasicGeometryNode::setGeometry(QSGGeometry *geometry)
{
    if ((flags() & OwnsGeometry) != 0 && m_geometry != geometry)
        delete m_geometry;
    m_geometry = geometry;
    markDirty(DirtyGeometry);
}



/*!
    \class QSGGeometryNode
    \brief The QSGGeometryNode class is used for all rendered content in the scene graph.

    \inmodule QtQuick
    \ingroup qtquick-scenegraph-nodes

    The QSGGeometryNode consists of geometry and material. The geometry defines the mesh,
    the vertices and their structure, to be drawn. The Material defines how the shape is
    filled.

    The following is a code snippet illustrating how to create a red
    line using a QSGGeometryNode:
    \code
        QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 2);
        geometry->setDrawingMode(GL_LINES);
        geometry->setLineWidth(3);
        geometry->vertexDataAsPoint2D()[0].set(0, 0);
        geometry->vertexDataAsPoint2D()[1].set(width(), height());

        QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
        material->setColor(QColor(255, 0, 0));

        QSGGeometryNode *node = new QSGGeometryNode;
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    \endcode

    A geometry node must have both geometry and a normal material before it is added to
    the scene graph. When the geometry and materials are changed after the node has
    been added to the scene graph, the user should also mark them as dirty using
    QSGNode::markDirty().

    The geometry node supports two types of materials, the opaqueMaterial and the normal
    material. The opaqueMaterial is used when the accumulated scene graph opacity at the
    time of rendering is 1. The primary usecase is to special case opaque rendering
    to avoid an extra operation in the fragment shader can have significant performance
    impact on embedded graphics chips. The opaque material is optional.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.

    \sa QSGGeometry, QSGMaterial, QSGSimpleMaterial
 */


/*!
    Creates a new geometry node without geometry and material.
 */

QSGGeometryNode::QSGGeometryNode()
    : QSGBasicGeometryNode(GeometryNodeType)
    , m_render_order(0)
    , m_material(0)
    , m_opaque_material(0)
    , m_opacity(1)
{
}


/*!
    \internal
 */
QSGGeometryNode::QSGGeometryNode(QSGGeometryNodePrivate &dd)
    : QSGBasicGeometryNode(dd, GeometryNodeType)
    , m_render_order(0)
    , m_material(0)
    , m_opaque_material(0)
    , m_opacity(1)
{
}


/*!
    Deletes this geometry node.

    The flags QSGNode::OwnsMaterial, QSGNode::OwnsOpaqueMaterial and
    QSGNode::OwnsGeometry decides weither the geometry node should also
    delete the materials and geometry. By default, these flags are disabled.
 */

QSGGeometryNode::~QSGGeometryNode()
{
    if (flags() & OwnsMaterial)
        delete m_material;
    if (flags() & OwnsOpaqueMaterial)
        delete m_opaque_material;
}



/*!
    \fn int QSGGeometryNode::renderOrder() const

    Returns the render order of this geometry node.

    \internal
 */

/*!
    \fn QSGMaterial *QSGGeometryNode::material() const

    Returns the material of the QSGGeometryNode.

    \sa setMaterial()
 */

/*!
    \fn QSGMaterial *QSGGeometryNode::opaqueMaterial() const

    Returns the opaque material of the QSGGeometryNode.

    \sa setOpaqueMaterial()
 */

/*!
    \fn qreal QSGGeometryNode::inheritedOpacity() const

    Set during rendering to specify the inherited opacity for that
    rendering pass.

    \internal
 */


/*!
    Sets the render order of this node to be \a order.

    Geometry nodes are rendered in an order that visually looks like
    low order nodes are rendered prior to high order nodes. For opaque
    geometry there is little difference as z-testing will handle
    the discard, but for translucent objects, the rendering should
    normally be specified in the order of back-to-front.

    The default render order is \c 0.

    \internal
  */
void QSGGeometryNode::setRenderOrder(int order)
{
    m_render_order = order;
}



/*!
    Sets the material of this geometry node to \a material.

    Geometry nodes must have a material before they can be added to the
    scene graph.

    If the material is changed whitout calling setMaterial() again, the user
    must also mark the material as dirty using QSGNode::markDirty().

 */
void QSGGeometryNode::setMaterial(QSGMaterial *material)
{
    if ((flags() & OwnsMaterial) != 0 && m_material != material)
        delete m_material;
    m_material = material;
#ifndef QT_NO_DEBUG
    if (m_material != 0 && m_opaque_material == m_material)
        qWarning("QSGGeometryNode: using same material for both opaque and translucent");
#endif
    markDirty(DirtyMaterial);
}



/*!
    Sets the opaque material of this geometry to \a material.

    The opaque material will be preferred by the renderer over the
    default material, as returned by the material() function, if
    it is not null and the geometry item has an inherited opacity of
    1.

    The opaqueness refers to scene graph opacity, the material is still
    allowed to set QSGMaterial::Blending to true and draw transparent
    pixels.

    If the material is changed whitout calling setOpaqueMaterial()
    again, the user must also mark the opaque material as dirty using
    QSGNode::markDirty().

 */
void QSGGeometryNode::setOpaqueMaterial(QSGMaterial *material)
{
    if ((flags() & OwnsOpaqueMaterial) != 0 && m_opaque_material != m_material)
        delete m_opaque_material;
    m_opaque_material = material;
#ifndef QT_NO_DEBUG
    if (m_opaque_material != 0 && m_opaque_material == m_material)
        qWarning("QSGGeometryNode: using same material for both opaque and translucent");
#endif

    markDirty(DirtyMaterial);
}



/*!
    Returns the material which should currently be used for geometry node.

    If the inherited opacity of the node is 1 and there is an opaque material
    set on this node, it will be returned; otherwise, the default material
    will be returned.

    \warning This function requires the scene graph above this item to be
    completely free of dirty states, so it can only be called during rendering

    \internal

    \sa setMaterial, setOpaqueMaterial
 */
QSGMaterial *QSGGeometryNode::activeMaterial() const
{
    if (m_opaque_material && m_opacity > 0.999)
        return m_opaque_material;
    return m_material;
}


/*!
    Sets the inherited opacity of this geometry to \a opacity.

    This function is meant to be called by the node preprocessing
    prior to rendering the tree, so it will not mark the tree as
    dirty.

    \internal
  */
void QSGGeometryNode::setInheritedOpacity(qreal opacity)
{
    Q_ASSERT(opacity >= 0 && opacity <= 1);
    m_opacity = opacity;
}


/*!
    \class QSGClipNode
    \brief The QSGClipNode class implements the clipping functionality in the scene graph.

    \inmodule QtQuick
    \ingroup qtquick-scenegraph-nodes

    Clipping applies to the node's subtree and can be nested. Multiple clip nodes will be
    accumulated by intersecting all their geometries. The accumulation happens
    as part of the rendering.

    Clip nodes must have a geometry before they can be added to the scene graph.

    Clipping is usually implemented by using the stencil buffer.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.
 */



/*!
    Creates a new QSGClipNode without a geometry.

    The clip node must have a geometry before it can be added to the
    scene graph.
 */

QSGClipNode::QSGClipNode()
    : QSGBasicGeometryNode(ClipNodeType)
{
    Q_UNUSED(m_reserved);
}



/*!
    Deletes this QSGClipNode.

    If the flag QSGNode::OwnsGeometry is set, the geometry will also be
    deleted.
 */

QSGClipNode::~QSGClipNode()
{
}



/*!
    \fn bool QSGClipNode::isRectangular() const

    Returns if this clip node has a rectangular clip.
 */



/*!
    Sets whether this clip node has a rectangular clip to \a rectHint.

    This is an optimization hint which means that the renderer can
    use scissoring instead of stencil, which is significantly faster.

    When this hint is set and it is applicable, the clip region will be
    generated from clipRect() rather than geometry().
 */

void QSGClipNode::setIsRectangular(bool rectHint)
{
    m_is_rectangular = rectHint;
}



/*!
    \fn QRectF QSGClipNode::clipRect() const

    Returns the clip rect of this node.
 */


/*!
    Sets the clip rect of this clip node to \a rect.

    When a rectangular clip is set in combination with setIsRectangular
    the renderer may in some cases use a more optimal clip method.
 */
void QSGClipNode::setClipRect(const QRectF &rect)
{
    m_clip_rect = rect;
}


/*!
    \class QSGTransformNode
    \brief The QSGTransformNode class implements transformations in the scene graph

    \inmodule QtQuick
    \ingroup qtquick-scenegraph-nodes

    Transformations apply the node's subtree and can be nested. Multiple transform nodes
    will be accumulated by intersecting all their matrices. The accumulation happens
    as part of the rendering.

    The transform nodes implement a 4x4 matrix which in theory supports full 3D
    transformations. However, because the renderer optimizes for 2D use-cases rather
    than 3D use-cases, rendering a scene with full 3D transformations needs to
    be done with some care.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.

 */


/*!
    Create a new QSGTransformNode with its matrix set to the identity matrix.
 */

QSGTransformNode::QSGTransformNode()
    : QSGNode(TransformNodeType)
{
}



/*!
    Deletes this transform node.
 */

QSGTransformNode::~QSGTransformNode()
{
}



/*!
    \fn QMatrix4x4 QSGTransformNode::matrix() const

    Returns this transform node's matrix.
 */



/*!
    Sets this transform node's matrix to \a matrix.
 */

void QSGTransformNode::setMatrix(const QMatrix4x4 &matrix)
{
    m_matrix = matrix;
    markDirty(DirtyMatrix);
}

/*!
    \fn const QMatrix4x4 &QSGTransformNode::combinedMatrix() const

    Set during rendering to the combination of all parent matrices for
    that rendering pass.

    \internal
 */



/*!
    Sets the combined matrix of this matrix to \a transform.

    This function is meant to be called by the node preprocessing
    prior to rendering the tree, so it will not mark the tree as
    dirty.

    \internal
  */
void QSGTransformNode::setCombinedMatrix(const QMatrix4x4 &matrix)
{
    m_combined_matrix = matrix;
}



/*!
    \class QSGRootNode
    \brief The QSGRootNode is the toplevel root of any scene graph.

    The root node is used to attach a scene graph to a renderer.

    \internal
 */



/*!
    \fn QSGRootNode::QSGRootNode()

    Creates a new root node.
 */

QSGRootNode::QSGRootNode()
    : QSGNode(RootNodeType)
{
}


/*!
    Deletes the root node.

    When a root node is deleted it removes itself from all of renderers
    that are referencing it.
 */

QSGRootNode::~QSGRootNode()
{
    while (!m_renderers.isEmpty())
        m_renderers.constLast()->setRootNode(0);
    destroy(); // Must call destroy() here because markDirty() casts this to QSGRootNode.
}



/*!
    Called to notify all renderers that \a node has been marked as dirty
    with \a flags.
 */

void QSGRootNode::notifyNodeChange(QSGNode *node, DirtyState state)
{
    for (int i=0; i<m_renderers.size(); ++i) {
        m_renderers.at(i)->nodeChanged(node, state);
    }
}



/*!
    \class QSGOpacityNode
    \brief The QSGOpacityNode class is used to change opacity of nodes.

    \inmodule QtQuick
    \ingroup qtquick-scenegraph-nodes

    Opacity applies to its subtree and can be nested. Multiple opacity nodes
    will be accumulated by multiplying their opacity. The accumulation happens
    as part of the rendering.

    When nested opacity gets below a certain threshold, the subtree might
    be marked as blocked, causing isSubtreeBlocked() to return true. This
    is done for performance reasons.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.
 */



/*!
    Constructs an opacity node with a default opacity of 1.

    Opacity accumulates downwards in the scene graph so a node with two
    QSGOpacityNode instances above it, both with opacity of 0.5, will have
    effective opacity of 0.25.

    The default opacity of nodes is 1.
  */
QSGOpacityNode::QSGOpacityNode()
    : QSGNode(OpacityNodeType)
    , m_opacity(1)
    , m_combined_opacity(1)
{
}



/*!
    Deletes the opacity node.
 */

QSGOpacityNode::~QSGOpacityNode()
{
}



/*!
    \fn qreal QSGOpacityNode::opacity() const

    Returns this opacity node's opacity.
 */

const qreal OPACITY_THRESHOLD = 0.001;

/*!
    Sets the opacity of this node to \a opacity.

    Before rendering the graph, the renderer will do an update pass
    over the subtree to propagate the opacity to its children.

    The value will be bounded to the range 0 to 1.
 */

void QSGOpacityNode::setOpacity(qreal opacity)
{
    opacity = qBound<qreal>(0, opacity, 1);
    if (m_opacity == opacity)
        return;
    DirtyState dirtyState = DirtyOpacity;

    if ((m_opacity < OPACITY_THRESHOLD && opacity >= OPACITY_THRESHOLD)     // blocked to unblocked
        || (m_opacity >= OPACITY_THRESHOLD && opacity < OPACITY_THRESHOLD)) // unblocked to blocked
        dirtyState |= DirtySubtreeBlocked;

    m_opacity = opacity;
    markDirty(dirtyState);
}



/*!
    \fn qreal QSGOpacityNode::combinedOpacity() const

    Returns this node's accumulated opacity.

    This vaule is calculated during rendering and only stored
    in the opacity node temporarily.

    \internal
 */



/*!
    Sets the combined opacity of this node to \a opacity.

    This function is meant to be called by the node preprocessing
    prior to rendering the tree, so it will not mark the tree as
    dirty.

    \internal
 */

void QSGOpacityNode::setCombinedOpacity(qreal opacity)
{
    m_combined_opacity = opacity;
}



/*!
    For performance reasons, we block the subtree when the opacity
    is below a certain threshold.

    \internal
 */

bool QSGOpacityNode::isSubtreeBlocked() const
{
    return m_opacity < OPACITY_THRESHOLD;
}


/*!
    \class QSGNodeVisitor
    \brief The QSGNodeVisitor class is a helper class for traversing the scene graph.

    \internal
 */

QSGNodeVisitor::~QSGNodeVisitor()
{

}


void QSGNodeVisitor::visitNode(QSGNode *n)
{
    switch (n->type()) {
    case QSGNode::TransformNodeType: {
        QSGTransformNode *t = static_cast<QSGTransformNode *>(n);
        enterTransformNode(t);
        visitChildren(t);
        leaveTransformNode(t);
        break; }
    case QSGNode::GeometryNodeType: {
        QSGGeometryNode *g = static_cast<QSGGeometryNode *>(n);
        enterGeometryNode(g);
        visitChildren(g);
        leaveGeometryNode(g);
        break; }
    case QSGNode::ClipNodeType: {
        QSGClipNode *c = static_cast<QSGClipNode *>(n);
        enterClipNode(c);
        visitChildren(c);
        leaveClipNode(c);
        break; }
    case QSGNode::OpacityNodeType: {
        QSGOpacityNode *o = static_cast<QSGOpacityNode *>(n);
        enterOpacityNode(o);
        visitChildren(o);
        leaveOpacityNode(o);
        break; }
    default:
        visitChildren(n);
        break;
    }
}

void QSGNodeVisitor::visitChildren(QSGNode *n)
{
    for (QSGNode *c = n->firstChild(); c; c = c->nextSibling())
        visitNode(c);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug d, const QSGGeometryNode *n)
{
    if (!n) {
        d << "Geometry(null)";
        return d;
    }
    d << "GeometryNode(" << hex << (const void *) n << dec;

    const QSGGeometry *g = n->geometry();

    if (!g) {
        d << "no geometry";
    } else {

        switch (g->drawingMode()) {
        case QSGGeometry::DrawTriangleStrip: d << "strip"; break;
        case QSGGeometry::DrawTriangleFan: d << "fan"; break;
        case QSGGeometry::DrawTriangles: d << "triangles"; break;
        default: break;
        }

        d << "#V:" << g->vertexCount() << "#I:" << g->indexCount();

        if (g->attributeCount() > 0 && g->attributes()->type == QSGGeometry::FloatType) {
            float x1 = 1e10, x2 = -1e10, y1=1e10, y2=-1e10;
            int stride = g->sizeOfVertex();
            for (int i = 0; i < g->vertexCount(); ++i) {
                float x = ((float *)((char *)const_cast<QSGGeometry *>(g)->vertexData() + i * stride))[0];
                float y = ((float *)((char *)const_cast<QSGGeometry *>(g)->vertexData() + i * stride))[1];

                x1 = qMin(x1, x);
                x2 = qMax(x2, x);
                y1 = qMin(y1, y);
                y2 = qMax(y2, y);
            }

            d << "x1=" << x1 << "y1=" << y1 << "x2=" << x2 << "y2=" << y2;
        }
    }

    if (n->material())
        d << "materialtype=" << n->material()->type();


    d << ')';
#ifdef QSG_RUNTIME_DESCRIPTION
    d << QSGNodePrivate::description(n);
#endif
    return d;
}

QDebug operator<<(QDebug d, const QSGClipNode *n)
{
    if (!n) {
        d << "ClipNode(null)";
        return d;
    }
    d << "ClipNode(" << hex << (const void *) n << dec;

    if (n->childCount())
        d << "children=" << n->childCount();

    d << "is rect?" << (n->isRectangular() ? "yes" : "no");

    d << ')';
#ifdef QSG_RUNTIME_DESCRIPTION
    d << QSGNodePrivate::description(n);
#endif
    d << (n->isSubtreeBlocked() ? "*BLOCKED*" : "");
    return d;
}

QDebug operator<<(QDebug d, const QSGTransformNode *n)
{
    if (!n) {
        d << "TransformNode(null)";
        return d;
    }
    const QMatrix4x4 m = n->matrix();
    d << "TransformNode(";
    d << hex << (const void *) n << dec;
    if (m.isIdentity())
        d << "identity";
    else if (m.determinant() == 1 && m(0, 0) == 1 && m(1, 1) == 1 && m(2, 2) == 1)
        d << "translate" << m(0, 3) << m(1, 3) << m(2, 3);
    else
        d << "det=" << n->matrix().determinant();
#ifdef QSG_RUNTIME_DESCRIPTION
    d << QSGNodePrivate::description(n);
#endif
    d << (n->isSubtreeBlocked() ? "*BLOCKED*" : "");
    d << ')';
    return d;
}

QDebug operator<<(QDebug d, const QSGOpacityNode *n)
{
    if (!n) {
        d << "OpacityNode(null)";
        return d;
    }
    d << "OpacityNode(";
    d << hex << (const void *) n << dec;
    d << "opacity=" << n->opacity()
      << "combined=" << n->combinedOpacity()
      << (n->isSubtreeBlocked() ? "*BLOCKED*" : "");
#ifdef QSG_RUNTIME_DESCRIPTION
    d << QSGNodePrivate::description(n);
#endif
    d << ')';
    return d;
}


QDebug operator<<(QDebug d, const QSGRootNode *n)
{
    if (!n) {
        d << "RootNode(null)";
        return d;
    }
    d << "RootNode" << hex << (const void *) n << (n->isSubtreeBlocked() ? "*BLOCKED*" : "");
#ifdef QSG_RUNTIME_DESCRIPTION
    d << QSGNodePrivate::description(n);
#endif
    d << ')';
    return d;
}



QDebug operator<<(QDebug d, const QSGNode *n)
{
    if (!n) {
        d << "Node(null)";
        return d;
    }
    switch (n->type()) {
    case QSGNode::GeometryNodeType:
        d << static_cast<const QSGGeometryNode *>(n);
        break;
    case QSGNode::TransformNodeType:
        d << static_cast<const QSGTransformNode *>(n);
        break;
    case QSGNode::ClipNodeType:
        d << static_cast<const QSGClipNode *>(n);
        break;
    case QSGNode::RootNodeType:
        d << static_cast<const QSGRootNode *>(n);
        break;
    case QSGNode::OpacityNodeType:
        d << static_cast<const QSGOpacityNode *>(n);
        break;
    case QSGNode::RenderNodeType:
        d << "RenderNode(" << hex << (const void *) n << dec
          << "flags=" << (int) n->flags() << dec
          << (n->isSubtreeBlocked() ? "*BLOCKED*" : "");
#ifdef QSG_RUNTIME_DESCRIPTION
        d << QSGNodePrivate::description(n);
#endif
        d << ')';
        break;
    default:
        d << "Node(" << hex << (const void *) n << dec
          << "flags=" << (int) n->flags() << dec
          << (n->isSubtreeBlocked() ? "*BLOCKED*" : "");
#ifdef QSG_RUNTIME_DESCRIPTION
        d << QSGNodePrivate::description(n);
#endif
        d << ')';
        break;
    }
    return d;
}

#endif

QT_END_NAMESPACE
