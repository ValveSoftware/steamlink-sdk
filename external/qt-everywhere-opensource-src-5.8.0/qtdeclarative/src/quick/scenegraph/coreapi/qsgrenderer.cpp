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

#include "qsgrenderer_p.h"
#include "qsgnodeupdater_p.h"
#ifndef QT_NO_OPENGL
# include <QtGui/QOpenGLFramebufferObject>
# include <QtGui/QOpenGLContext>
# include <QtGui/QOpenGLFunctions>
#endif
#include <private/qquickprofiler_p.h>

#include <QtCore/QElapsedTimer>

QT_BEGIN_NAMESPACE

static const bool qsg_sanity_check = qEnvironmentVariableIntValue("QSG_SANITY_CHECK");

static QElapsedTimer frameTimer;
static qint64 preprocessTime;
static qint64 updatePassTime;

int qt_sg_envInt(const char *name, int defaultValue)
{
    if (Q_LIKELY(!qEnvironmentVariableIsSet(name)))
        return defaultValue;
    bool ok = false;
    int value = qgetenv(name).toInt(&ok);
    return ok ? value : defaultValue;
}

void QSGBindable::clear(QSGAbstractRenderer::ClearMode mode) const
{
#ifndef QT_NO_OPENGL
    GLuint bits = 0;
    if (mode & QSGAbstractRenderer::ClearColorBuffer) bits |= GL_COLOR_BUFFER_BIT;
    if (mode & QSGAbstractRenderer::ClearDepthBuffer) bits |= GL_DEPTH_BUFFER_BIT;
    if (mode & QSGAbstractRenderer::ClearStencilBuffer) bits |= GL_STENCIL_BUFFER_BIT;
    QOpenGLContext::currentContext()->functions()->glClear(bits);
#else
    Q_UNUSED(mode)
#endif
}

// Reactivate the color buffer after switching to the stencil.
void QSGBindable::reactivate() const
{
#ifndef QT_NO_OPENGL
    QOpenGLContext::currentContext()->functions()->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif
}
#ifndef QT_NO_OPENGL
QSGBindableFboId::QSGBindableFboId(GLuint id)
    : m_id(id)
{
}


void QSGBindableFboId::bind() const
{
    QOpenGLContext::currentContext()->functions()->glBindFramebuffer(GL_FRAMEBUFFER, m_id);
}
#endif
/*!
    \class QSGRenderer
    \brief The renderer class is the abstract baseclass use for rendering the
    QML scene graph.

    The renderer is not tied to any particular surface. It expects a context to
    be current and will render into that surface according to how the device rect,
    viewport rect and projection transformation are set up.

    Rendering is a sequence of steps initiated by calling renderScene(). This will
    effectively draw the scene graph starting at the root node. The QSGNode::preprocess()
    function will be called for all the nodes in the graph, followed by an update
    pass which updates all matrices, opacity, clip states and similar in the graph.
    Because the update pass is called after preprocess, it is safe to modify the graph
    during preprocess. To run a custom update pass over the graph, install a custom
    QSGNodeUpdater using setNodeUpdater(). Once all the graphs dirty states are updated,
    the virtual render() function is called.

    The render() function is implemented by QSGRenderer subclasses to render the graph
    in the most optimal way for a given hardware.

    The renderer can make use of stencil, depth and color buffers in addition to the
    scissor rect.

    \internal
 */


QSGRenderer::QSGRenderer(QSGRenderContext *context)
    : m_current_opacity(1)
    , m_current_determinant(1)
    , m_device_pixel_ratio(1)
    , m_context(context)
    , m_node_updater(0)
    , m_bindable(0)
    , m_changed_emitted(false)
    , m_is_rendering(false)
{
}


QSGRenderer::~QSGRenderer()
{
    setRootNode(0);
    delete m_node_updater;
}

/*!
    Returns the node updater that this renderer uses to update states in the
    scene graph.

    If no updater is specified a default one is constructed.
 */

QSGNodeUpdater *QSGRenderer::nodeUpdater() const
{
    if (!m_node_updater)
        const_cast<QSGRenderer *>(this)->m_node_updater = new QSGNodeUpdater();
    return m_node_updater;
}


/*!
    Sets the node updater that this renderer uses to update states in the
    scene graph.

    This will delete and override any existing node updater
  */
void QSGRenderer::setNodeUpdater(QSGNodeUpdater *updater)
{
    if (m_node_updater)
        delete m_node_updater;
    m_node_updater = updater;
}

bool QSGRenderer::isMirrored() const
{
    QMatrix4x4 matrix = projectionMatrix();
    // Mirrored relative to the usual Qt coordinate system with origin in the top left corner.
    return matrix(0, 0) * matrix(1, 1) - matrix(0, 1) * matrix(1, 0) > 0;
}

void QSGRenderer::renderScene(uint fboId)
{
#ifndef QT_NO_OPENGL
    if (fboId) {
        QSGBindableFboId bindable(fboId);
        renderScene(bindable);
    } else {
        class B : public QSGBindable
        {
        public:
            void bind() const { QOpenGLFramebufferObject::bindDefault(); }
        } bindable;
        renderScene(bindable);
    }
#else
    Q_UNUSED(fboId)
#endif
}

void QSGRenderer::renderScene(const QSGBindable &bindable)
{
    if (!rootNode())
        return;

    m_is_rendering = true;


    bool profileFrames = QSG_LOG_TIME_RENDERER().isDebugEnabled();
    if (profileFrames)
        frameTimer.start();
    Q_QUICK_SG_PROFILE_START(QQuickProfiler::SceneGraphRendererFrame);

    qint64 bindTime = 0;
    qint64 renderTime = 0;

    m_bindable = &bindable;
    preprocess();

    bindable.bind();
    if (profileFrames)
        bindTime = frameTimer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphRendererFrame,
                              QQuickProfiler::SceneGraphRendererBinding);

#ifndef QT_NO_OPENGL
    // Sanity check that attribute registers are disabled
    if (qsg_sanity_check) {
        GLint count = 0;
        QOpenGLContext::currentContext()->functions()->glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &count);
        GLint enabled;
        for (int i=0; i<count; ++i) {
            QOpenGLContext::currentContext()->functions()->glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
            if (enabled) {
                qWarning("QSGRenderer: attribute %d is enabled, this can lead to memory corruption and crashes.", i);
            }
        }
    }
#endif

    render();
    if (profileFrames)
        renderTime = frameTimer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_END(QQuickProfiler::SceneGraphRendererFrame,
                           QQuickProfiler::SceneGraphRendererRender);

    m_is_rendering = false;
    m_changed_emitted = false;
    m_bindable = 0;

    qCDebug(QSG_LOG_TIME_RENDERER,
            "time in renderer: total=%dms, preprocess=%d, updates=%d, binding=%d, rendering=%d",
            int(renderTime / 1000000),
            int(preprocessTime / 1000000),
            int((updatePassTime - preprocessTime) / 1000000),
            int((bindTime - updatePassTime) / 1000000),
            int((renderTime - bindTime) / 1000000));
}

/*!
    Updates internal data structures and emits the sceneGraphChanged() signal.

    If \a flags contains the QSGNode::DirtyNodeRemoved flag, the node might be
    in the process of being destroyed. It is then not safe to downcast the node
    pointer.
*/

void QSGRenderer::nodeChanged(QSGNode *node, QSGNode::DirtyState state)
{
    if (state & QSGNode::DirtyNodeAdded)
        addNodesToPreprocess(node);
    if (state & QSGNode::DirtyNodeRemoved)
        removeNodesToPreprocess(node);
    if (state & QSGNode::DirtyUsePreprocess) {
        if (node->flags() & QSGNode::UsePreprocess)
            m_nodes_to_preprocess.insert(node);
        else
            m_nodes_to_preprocess.remove(node);
    }

    if (!m_changed_emitted && !m_is_rendering) {
        // Premature overoptimization to avoid excessive signal emissions
        m_changed_emitted = true;
        emit sceneGraphChanged();
    }
}

void QSGRenderer::preprocess()
{
    QSGRootNode *root = rootNode();
    Q_ASSERT(root);

    // We need to take a copy here, in case any of the preprocess calls deletes a node that
    // is in the preprocess list and thus, changes the m_nodes_to_preprocess behind our backs
    // For the default case, when this does not happen, the cost is neglishible.
    QSet<QSGNode *> items = m_nodes_to_preprocess;

    for (QSet<QSGNode *>::const_iterator it = items.constBegin();
         it != items.constEnd(); ++it) {
        QSGNode *n = *it;
        if (!nodeUpdater()->isNodeBlocked(n, root)) {
            n->preprocess();
        }
    }

    bool profileFrames = QSG_LOG_TIME_RENDERER().isDebugEnabled();
    if (profileFrames)
        preprocessTime = frameTimer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphRendererFrame,
                              QQuickProfiler::SceneGraphRendererPreprocess);

    nodeUpdater()->updateStates(root);

    if (profileFrames)
        updatePassTime = frameTimer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphRendererFrame,
                              QQuickProfiler::SceneGraphRendererUpdate);
}

void QSGRenderer::addNodesToPreprocess(QSGNode *node)
{
    for (QSGNode *c = node->firstChild(); c; c = c->nextSibling())
        addNodesToPreprocess(c);
    if (node->flags() & QSGNode::UsePreprocess)
        m_nodes_to_preprocess.insert(node);
}

void QSGRenderer::removeNodesToPreprocess(QSGNode *node)
{
    for (QSGNode *c = node->firstChild(); c; c = c->nextSibling())
        removeNodesToPreprocess(c);
    if (node->flags() & QSGNode::UsePreprocess)
        m_nodes_to_preprocess.remove(node);
}


/*!
    \class QSGNodeDumper
    \brief The QSGNodeDumper class provides a way of dumping a scene grahp to the console.

    This class is solely for debugging purposes.

    \internal
 */

void QSGNodeDumper::dump(QSGNode *n)
{
    QSGNodeDumper dump;
    dump.visitNode(n);
}

void QSGNodeDumper::visitNode(QSGNode *n)
{
    qDebug() << QByteArray(m_indent * 2, ' ').constData() << n;
    QSGNodeVisitor::visitNode(n);
}

void QSGNodeDumper::visitChildren(QSGNode *n)
{
    ++m_indent;
    QSGNodeVisitor::visitChildren(n);
    --m_indent;
}


QT_END_NAMESPACE
