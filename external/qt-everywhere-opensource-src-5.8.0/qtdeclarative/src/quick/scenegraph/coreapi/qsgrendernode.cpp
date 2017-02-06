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

#include "qsgrendernode.h"
#include "qsgrendernode_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QSGRenderNode
    \brief The QSGRenderNode class represents a set of custom rendering commands
    targeting the graphics API that is in use by the scenegraph.
    \inmodule QtQuick
    \since 5.8
 */

QSGRenderNode::QSGRenderNode()
    : QSGNode(RenderNodeType),
      d(new QSGRenderNodePrivate)
{
}

/*!
    Destructs the render node. Derived classes are expected to perform cleanup
    similar to releaseResources() in here.

    When a low-level graphics API is in use, the scenegraph will make sure
    there is a CPU-side wait for the GPU to complete all work submitted to the
    scenegraph's graphics command queue before the scenegraph's nodes are
    deleted. Therefore there is no need to issue additional waits here, unless
    the render() implementation is using additional command queues.

    \sa releaseResources()
 */
QSGRenderNode::~QSGRenderNode()
{
    delete d;
}

QSGRenderNodePrivate::QSGRenderNodePrivate()
    : m_matrix(0)
    , m_clip_list(0)
    , m_opacity(1)
{
}

/*!
    This function should return a mask where each bit represents graphics states changed by
    the \l render() function:
    \list
    \li DepthState - depth write mask, depth test enabled, depth comparison function
    \li StencilState - stencil write masks, stencil test enabled, stencil operations,
                      stencil comparison functions
    \li ScissorState - scissor enabled, scissor test enabled
    \li ColorState - clear color, color write mask
    \li BlendState - blend enabled, blend function
    \li CullState - front face, cull face enabled
    \li ViewportState - viewport
    \li RenderTargetState - render target
    \endlist

    The function is called by the renderer so it can reset the states after
    rendering this node. This makes the implementation of render() simpler
    since it does not have to query and restore these states.

    The default implementation returns 0, meaning no relevant state was changed
    in render().

    With APIs other than OpenGL the relevant states are only those that are set
    via the command list (for example, OMSetRenderTargets, RSSetViewports,
    RSSetScissorRects, OMSetBlendFactor, OMSetStencilRef in case of D3D12), and
    only when such commands were added to the scenegraph's command list queried
    via the QSGRendererInterface::CommandList resource enum. States set in
    pipeline state objects do not need to be reported here. Similarly, draw
    call related settings (root signature, descriptor heaps, etc.) are always
    set again by the scenegraph so render() can freely change them.

    The software backend exposes its QPainter and saves and restores before and
    after invoking render(). Therefore reporting any changed states from here
    is not necessary.

    \note This function may be called before render().
  */
QSGRenderNode::StateFlags QSGRenderNode::changedStates() const
{
    return 0;
}

/*!
    \fn void QSGRenderNode::render(const RenderState *state)

    This function is called by the renderer and should paint this node with
    directly invoking commands in the graphics API (OpenGL, Direct3D, etc.)
    currently in use.

    The effective opacity can be retrieved with \l inheritedOpacity().

    The projection matrix is available through \a state, while the model-view
    matrix can be fetched with \l matrix(). The combined matrix is then the
    projection matrix times the model-view matrix. The correct stacking of the
    items in the scene is ensured by the projection matrix.

    When using the provided matrices, the coordinate system for vertex data
    follows the usual QQuickItem conventions: top-left is (0, 0), bottom-right
    is the corresponding QQuickItem's width() and height() minus one. For
    example, assuming a two float (x-y) per vertex coordinate layout, a
    triangle covering half of the item can be specified as (width - 1, height - 1),
    (0, 0), (0, height - 1) using counter-clockwise direction.

    \note QSGRenderNode is provided as a means to implement custom 2D or 2.5D
    Qt Quick items. It is not intended for integrating true 3D content into the
    Qt Quick scene. That use case is better supported by
    QQuickFramebufferObject, QQuickWindow::beforeRendering(), or the
    equivalents of those for APIs other than OpenGL.

    Clip information is calculated before the function is called, it is however
    not enabled. Implementations wishing to take clipping into account can set
    up scissoring or stencil based on the information in \a state. Some
    scenegraph backends, software in particular, use no scissor or stencil.
    There the clip region is provided as an ordinary QRegion.

    For OpenGL the following states are set on the render thread's context
    before this function is called:
    \list
    \li glDepthMask(false)
    \li glDisable(GL_DEPTH_TEST)
    \li glStencilMask(0)
    \li glEnable(GL_STENCIL_TEST)/glDisable(GL_STENCIL_TEST) depending on clip
    \li glStencilFunc(GL_EQUAL, state.stencilValue, 0xff) depending on clip
    \li glEnable(GL_SCISSOR_TEST)/glDisable(GL_SCISSOR_TEST) depending on clip
    \li glScissor(state.scissorRect.x(), state.scissorRect.y(),
                 state.scissorRect.width(), state.scissorRect.height()) depending on clip
    \li glEnable(GL_BLEND)
    \li glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)
    \li glDisable(GL_CULL_FACE)
    \endlist

    States that are not listed above, but are included in \l StateFlags, can
    have arbitrary values.

    \l changedStates() should return which states this function changes. If a
    state is not covered by \l StateFlags, the state should be set to the
    default value according to the OpenGL specification. For other APIs, see
    the documentation for changedStates() for more information.

    \note Depth writes are disabled when this function is called (for example,
    glDepthMask(false) in case of OpenGL). Enabling depth writes can lead to
    unexpected results, depending on the scenegraph backend in use, so nodes
    should avoid this.

    For APIs other than OpenGL, it will likely be necessary to query certain
    API-specific resources (for example, the graphics device or the command
    list/buffer to add the commands to). This is done via QSGRendererInterface.

    \sa QSGRendererInterface, QQuickWindow::rendererInterface()
  */

/*!
    This function is called when all custom graphics resources allocated by
    this node have to be freed immediately. In case the node does not directly
    allocate graphics resources (buffers, textures, render targets, fences,
    etc.) through the graphics API that is in use, there is nothing to do here.

    Failing to release all custom resources can lead to incorrect behavior in
    graphics device loss scenarios on some systems since subsequent
    reinitialization of the graphics system may fail.

    \note Some scenegraph backends may choose not to call this function.
    Therefore it is expected that QSGRenderNode implementations perform cleanup
    both in their destructor and in releaseResources().

    Unlike with the destructor, it is expected that render() can reinitialize
    all resources it needs when called after a call to releaseResources().

    With OpenGL, the scenegraph's OpenGL context will be current both when
    calling the destructor and this function.
 */
void QSGRenderNode::releaseResources()
{
}

/*!
    \enum QSGRenderNode::RenderingFlag

    Possible values for the bitmask returned from flags().

    \value BoundedRectRendering Indicates that the implementation of render()
    does not render outside the area reported from rect() in item
    coordinates. Such node implementations can lead to more efficient rendering,
    depending on the scenegraph backend. For example, the software backend can
    continue to use the more optimal partial update path when all render nodes
    in the scene have this flag set.

    \value DepthAwareRendering Indicates that the implementations of render()
    conforms to scenegraph expectations by only generating a Z value of 0 in
    scene coordinates which is then transformed by the matrices retrieved from
    RenderState::projectionMatrix() and matrix(), as described in the notes for
    render(). Such node implementations can lead to more efficient rendering,
    depending on the scenegraph backend. For example, the batching OpenGL
    renderer can continue to use a more optimal path when all render nodes in
    the scene have this flag set.

    \value OpaqueRendering Indicates that the implementation of render() writes
    out opaque pixels for the entire area reported from rect(). By default the
    renderers must assume that render() can also output semi or fully
    transparent pixels. Setting this flag can improve performance in some
    cases.

    \sa render(), rect()
 */

/*!
    \return flags describing the behavior of this render node.

    The default implementation returns 0.

    \sa RenderingFlag, rect()
 */
QSGRenderNode::RenderingFlags QSGRenderNode::flags() const
{
    return 0;
}

/*!
    \return the bounding rectangle in item coordinates for the area render()
    touches. The value is only in use when flags() includes
    BoundedRectRendering, ignored otherwise.

    Reporting the rectangle in combination with BoundedRectRendering is
    particularly important with the \c software backend because otherwise
    having a rendernode in the scene would trigger fullscreen updates, skipping
    all partial update optimizations.

    For rendernodes covering the entire area of a corresponding QQuickItem the
    return value will be (0, 0, item->width(), item->height()).

    \sa flags()
*/
QRectF QSGRenderNode::rect() const
{
    return QRectF();
}

/*!
    \return pointer to the current model-view matrix.
 */
const QMatrix4x4 *QSGRenderNode::matrix() const
{
    return d->m_matrix;
}

/*!
    \return the current clip list.
 */
const QSGClipNode *QSGRenderNode::clipList() const
{
    return d->m_clip_list;
}

/*!
    \return the current effective opacity.
 */
qreal QSGRenderNode::inheritedOpacity() const
{
    return d->m_opacity;
}

QSGRenderNode::RenderState::~RenderState()
{
}

/*!
    \fn const QMatrix4x4 *QSGRenderNode::RenderState::projectionMatrix() const

    \return pointer to the current projection matrix.

    The model-view matrix can be retrieved with QSGRenderNode::matrix().
    Typically \c{projection * modelview} is the matrix that is then used in the
    vertex shader to transform the vertices.
 */

/*!
    \fn const QMatrix4x4 *QSGRenderNode::RenderState::scissorRect() const

    \return the current scissor rectangle when clipping is active.

    \note Be aware of the differences between graphics APIs: for some the
    scissor rect is only active when scissoring is enabled (for example,
    OpenGL), while for others the scissor rect is equal to the viewport rect
    when there is no need to scissor away anything (for example, Direct3D 12).
 */

/*!
    \fn const QMatrix4x4 *QSGRenderNode::RenderState::scissorEnabled() const

    \return the current state of scissoring.

    \note Only relevant for graphics APIs that have a dedicated on/off state of
    scissoring.
 */

/*!
    \fn const QMatrix4x4 *QSGRenderNode::RenderState::stencilValue() const

    \return the current stencil reference value when clipping is active.
 */

/*!
    \fn const QMatrix4x4 *QSGRenderNode::RenderState::stencilEnabled() const

    \return the current state of stencil testing.

    \note With graphics APIs where stencil testing is enabled in pipeline state
    objects, instead of individual state-setting commands, it is up to the
    implementation of render() to enable stencil testing with operations
    \c KEEP, comparison function \c EQUAL, and a read and write mask of \c 0xFF.
 */

/*!
    \fn const QRegion *QSGRenderNode::clipRegion() const

    \return the current clip region or null for backends where clipping is
    implemented via stencil or scissoring.

    The software backend uses no projection, scissor or stencil, meaning most
    of the render state is not in use. However, the clip region that can be set
    on the QPainter still has to be communicated since reconstructing this
    manually in render() is not reasonable. It can therefore be queried via
    this function.
 */

/*!
    \return pointer to a \a state value.

    Reserved for future use.
 */
void *QSGRenderNode::RenderState::get(const char *state) const
{
    Q_UNUSED(state);
    return nullptr;
}

QT_END_NAMESPACE
