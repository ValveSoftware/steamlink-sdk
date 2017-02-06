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

#include "qquickframebufferobject.h"

#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOpenGLFunctions>
#include <private/qquickitem_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <qsgtextureprovider.h>

#include <QSGSimpleTextureNode>
#include <QSGRendererInterface>

QT_BEGIN_NAMESPACE

class QQuickFramebufferObjectPrivate : public QQuickItemPrivate
{
    Q_DECLARE_PUBLIC(QQuickFramebufferObject)
public:
    QQuickFramebufferObjectPrivate()
        : followsItemSize(true)
        , mirrorVertically(false)
        , node(0)
    {
    }

    bool followsItemSize;
    bool mirrorVertically;
    mutable QSGFramebufferObjectNode *node;
};

/*!
 * \class QQuickFramebufferObject
 * \inmodule QtQuick
 * \since 5.2
 *
 * \brief The QQuickFramebufferObject class is a convenience class
 * for integrating OpenGL rendering using a framebuffer object (FBO)
 * with Qt Quick.
 *
 * On most platforms, the rendering will occur on a \l {Scene Graph and Rendering}{dedicated thread}.
 * For this reason, the QQuickFramebufferObject class enforces a strict
 * separation between the item implementation and the FBO rendering. All
 * item logic, such as properties and UI-related helper functions needed by
 * QML should be located in a QQuickFramebufferObject class subclass.
 * Everything that relates to rendering must be located in the
 * QQuickFramebufferObject::Renderer class.
 *
 * To avoid race conditions and read/write issues from two threads
 * it is important that the renderer and the item never read or
 * write shared variables. Communication between the item and the renderer
 * should primarily happen via the
 * QQuickFramebufferObject::Renderer::synchronize() function. This function
 * will be called on the render thread while the GUI thread is blocked.
 *
 * Using queued connections or events for communication between item
 * and renderer is also possible.
 *
 * Both the Renderer and the FBO are memory managed internally.
 *
 * To render into the FBO, the user should subclass the Renderer class
 * and reimplement its Renderer::render() function. The Renderer subclass
 * is returned from createRenderer().
 *
 * The size of the FBO will by default adapt to the size of
 * the item. If a fixed size is preferred, set textureFollowsItemSize
 * to \c false and return a texture of your choosing from
 * QQuickFramebufferObject::Renderer::createFramebufferObject().
 *
 * Starting Qt 5.4, the QQuickFramebufferObject class is a
 * \l{QSGTextureProvider}{texture provider}
 * and can be used directly in \l {ShaderEffect}{ShaderEffects} and other
 * classes that consume texture providers.
 *
 * \sa {Scene Graph - Rendering FBOs}, {Scene Graph and Rendering}
 */

/*!
 * Constructs a new QQuickFramebufferObject with parent \a parent.
 */
QQuickFramebufferObject::QQuickFramebufferObject(QQuickItem *parent) :
    QQuickItem(*new QQuickFramebufferObjectPrivate, parent)
{
    setFlag(ItemHasContents);
}

/*!
 * \property QQuickFramebufferObject::textureFollowsItemSize
 *
 * This property controls if the size of the FBO's texture should follow
 * the dimensions of the QQuickFramebufferObject item. When this property
 * is false, the FBO will be created once the first time it is displayed.
 * If it is set to true, the FBO will be recreated every time the dimensions
 * of the item change.
 *
 * The default value is \c {true}.
 */

void QQuickFramebufferObject::setTextureFollowsItemSize(bool follows)
{
    Q_D(QQuickFramebufferObject);
    if (d->followsItemSize == follows)
        return;
    d->followsItemSize = follows;
    emit textureFollowsItemSizeChanged(d->followsItemSize);
}

bool QQuickFramebufferObject::textureFollowsItemSize() const
{
    Q_D(const QQuickFramebufferObject);
    return d->followsItemSize;
}

/*!
 * \property QQuickFramebufferObject::mirrorVertically
 *
 * This property controls if the size of the FBO's contents should be mirrored
 * vertically when drawing. This allows easy integration of third-party
 * rendering code that does not follow the standard expectations.
 *
 * The default value is \c {false}.
 *
 * \since 5.6
 */

void QQuickFramebufferObject::setMirrorVertically(bool enable)
{
    Q_D(QQuickFramebufferObject);
    if (d->mirrorVertically == enable)
        return;
    d->mirrorVertically = enable;
    emit mirrorVerticallyChanged(d->mirrorVertically);
    update();
}

bool QQuickFramebufferObject::mirrorVertically() const
{
    Q_D(const QQuickFramebufferObject);
    return d->mirrorVertically;
}

/*!
 * \internal
 */
void QQuickFramebufferObject::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    Q_D(QQuickFramebufferObject);
    if (newGeometry.size() != oldGeometry.size() && d->followsItemSize)
        update();
}

class QSGFramebufferObjectNode : public QSGTextureProvider, public QSGSimpleTextureNode
{
    Q_OBJECT

public:
    QSGFramebufferObjectNode()
        : window(0)
        , fbo(0)
        , msDisplayFbo(0)
        , renderer(0)
        , renderPending(true)
        , invalidatePending(false)
        , devicePixelRatio(1)
    {
        qsgnode_set_description(this, QStringLiteral("fbonode"));
    }

    ~QSGFramebufferObjectNode()
    {
        delete renderer;
        delete texture();
        delete fbo;
        delete msDisplayFbo;
    }

    void scheduleRender()
    {
        renderPending = true;
        window->update();
    }

    QSGTexture *texture() const Q_DECL_OVERRIDE
    {
        return QSGSimpleTextureNode::texture();
    }

public Q_SLOTS:
    void render()
    {
        if (renderPending) {
            renderPending = false;
            fbo->bind();
            QOpenGLContext::currentContext()->functions()->glViewport(0, 0, fbo->width(), fbo->height());
            renderer->render();
            fbo->bindDefault();

            if (msDisplayFbo)
                QOpenGLFramebufferObject::blitFramebuffer(msDisplayFbo, fbo);

            markDirty(QSGNode::DirtyMaterial);
            emit textureChanged();
        }
    }

    void handleScreenChange()
    {
        if (window->effectiveDevicePixelRatio() != devicePixelRatio) {
            renderer->invalidateFramebufferObject();
            quickFbo->update();
        }
    }

public:
    QQuickWindow *window;
    QOpenGLFramebufferObject *fbo;
    QOpenGLFramebufferObject *msDisplayFbo;
    QQuickFramebufferObject::Renderer *renderer;
    QQuickFramebufferObject *quickFbo;

    bool renderPending;
    bool invalidatePending;

    int devicePixelRatio;
};

static inline bool isOpenGL(QSGRenderContext *rc)
{
    QSGRendererInterface *rif = rc->sceneGraphContext()->rendererInterface(rc);
    return !rif || rif->graphicsApi() == QSGRendererInterface::OpenGL;
}

/*!
 * \internal
 */
QSGNode *QQuickFramebufferObject::updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
{
    QSGFramebufferObjectNode *n = static_cast<QSGFramebufferObjectNode *>(node);

    // We only abort if we never had a node before. This is so that we
    // don't recreate the renderer object if the thing becomes tiny. In
    // terms of API it would be horrible if the renderer would go away
    // that easily so with this logic, the renderer only goes away when
    // the scenegraph is invalidated or it is removed from the scene.
    if (!n && (width() <= 0 || height() <= 0))
        return 0;

    Q_D(QQuickFramebufferObject);

    if (!n) {
        if (!isOpenGL(d->sceneGraphRenderContext()))
            return 0;
        if (!d->node)
            d->node = new QSGFramebufferObjectNode;
        n = d->node;
    }

    if (!n->renderer) {
        n->window = window();
        n->renderer = createRenderer();
        n->renderer->data = n;
        n->quickFbo = this;
        connect(window(), SIGNAL(beforeRendering()), n, SLOT(render()));
        connect(window(), SIGNAL(screenChanged(QScreen*)), n, SLOT(handleScreenChange()));
    }

    n->renderer->synchronize(this);

    QSize minFboSize = d->sceneGraphContext()->minimumFBOSize();
    QSize desiredFboSize(qMax<int>(minFboSize.width(), width()),
                         qMax<int>(minFboSize.height(), height()));

    n->devicePixelRatio = window()->effectiveDevicePixelRatio();
    desiredFboSize *= n->devicePixelRatio;

    if (n->fbo && (d->followsItemSize || n->invalidatePending)) {
        if (n->fbo->size() != desiredFboSize) {
            delete n->fbo;
            n->fbo = 0;
            delete n->msDisplayFbo;
            n->msDisplayFbo = 0;
            n->invalidatePending = false;
        }
    }

    if (!n->fbo) {
        n->fbo = n->renderer->createFramebufferObject(desiredFboSize);

        GLuint displayTexture = n->fbo->texture();

        if (n->fbo->format().samples() > 0) {
            n->msDisplayFbo = new QOpenGLFramebufferObject(n->fbo->size());
            displayTexture = n->msDisplayFbo->texture();
        }

        n->setTexture(window()->createTextureFromId(displayTexture,
                                                    n->fbo->size(),
                                                    QQuickWindow::TextureHasAlphaChannel));
    }

    n->setTextureCoordinatesTransform(d->mirrorVertically ? QSGSimpleTextureNode::MirrorVertically : QSGSimpleTextureNode::NoTransform);
    n->setFiltering(d->smooth ? QSGTexture::Linear : QSGTexture::Nearest);
    n->setRect(0, 0, width(), height());

    n->scheduleRender();

    return n;
}

/*!
   \reimp
*/
bool QQuickFramebufferObject::isTextureProvider() const
{
    return true;
}

/*!
   \reimp
*/
QSGTextureProvider *QQuickFramebufferObject::textureProvider() const
{
    // When Item::layer::enabled == true, QQuickItem will be a texture
    // provider. In this case we should prefer to return the layer rather
    // than the fbo texture.
    if (QQuickItem::isTextureProvider())
        return QQuickItem::textureProvider();

    Q_D(const QQuickFramebufferObject);
    QQuickWindow *w = window();
    if (!w || !w->openglContext() || QThread::currentThread() != w->openglContext()->thread()) {
        qWarning("QQuickFramebufferObject::textureProvider: can only be queried on the rendering thread of an exposed window");
        return 0;
    }
    if (!isOpenGL(d->sceneGraphRenderContext()))
        return 0;
    if (!d->node)
        d->node = new QSGFramebufferObjectNode;
    return d->node;
}

/*!
   \reimp
*/
void QQuickFramebufferObject::releaseResources()
{
    // When release resources is called on the GUI thread, we only need to
    // forget about the node. Since it is the node we returned from updatePaintNode
    // it will be managed by the scene graph.
    Q_D(QQuickFramebufferObject);
    d->node = 0;
}

void QQuickFramebufferObject::invalidateSceneGraph()
{
    Q_D(QQuickFramebufferObject);
    d->node = 0;
}

/*!
 * \class QQuickFramebufferObject::Renderer
 * \inmodule QtQuick
 * \since 5.2
 *
 * The QQuickFramebufferObject::Renderer class is used to implement the
 * rendering logic of a QQuickFramebufferObject.
 */

/*!
 * Constructs a new renderer.
 *
 * This function is called during the scene graph sync phase when the
 * GUI thread is blocked.
 */
QQuickFramebufferObject::Renderer::Renderer()
    : data(0)
{
}

/*!
 * \fn QQuickFramebufferObject::Renderer *QQuickFramebufferObject::createRenderer() const
 *
 * Reimplement this function to create a renderer used to render into the FBO.
 *
 * This function will be called on the rendering thread while the GUI thread is
 * blocked.
 */

/*!
 * The Renderer is automatically deleted when the scene graph resources
 * for the QQuickFramebufferObject item is cleaned up.
 *
 * This function is called on the rendering thread.
 */
QQuickFramebufferObject::Renderer::~Renderer()
{
}

/*!
 * Returns the framebuffer object currently being rendered to.
 */
QOpenGLFramebufferObject *QQuickFramebufferObject::Renderer::framebufferObject() const
{
    return data ? ((QSGFramebufferObjectNode *) data)->fbo : 0;
}

/*!
 * \fn void QQuickFramebufferObject::Renderer::render()
 *
 * This function is called when the FBO should be rendered into. The framebuffer
 * is bound at this point and the \c glViewport has been set up to match
 * the FBO size.
 *
 * The FBO will be automatically unbound after the function returns.
 *
 * \note Do not assume that the OpenGL state is all set to the defaults when
 * this function is invoked, or that it is maintained between calls. Both the Qt
 * Quick renderer and the custom rendering code uses the same OpenGL
 * context. This means that the state might have been modified by Quick before
 * invoking this function.
 *
 * \note It is recommended to call QQuickWindow::resetOpenGLState() before
 * returning. This resets OpenGL state used by the Qt Quick renderer and thus
 * avoids interference from the state changes made by the rendering code in this
 * function.
 */

/*!
 * This function is called as a result of QQuickFramebufferObject::update().
 *
 * Use this function to update the renderer with changes that have occurred
 * in the item. \a item is the item that instantiated this renderer. The function
 * is called once before the FBO is created.
 *
 * \e {For instance, if the item has a color property which is controlled by
 * QML, one should call QQuickFramebufferObject::update() and use
 * synchronize() to copy the new color into the renderer so that it can be
 * used to render the next frame.}
 *
 * This function is the only place when it is safe for the renderer and the
 * item to read and write each others members.
 */
void QQuickFramebufferObject::Renderer::synchronize(QQuickFramebufferObject *item)
{
    Q_UNUSED(item);
}

/*!
 * Call this function during synchronize() to invalidate the current FBO. This
 * will result in a new FBO being created with createFramebufferObject().
 */
void QQuickFramebufferObject::Renderer::invalidateFramebufferObject()
{
    if (data)
        ((QSGFramebufferObjectNode *) data)->invalidatePending = true;
}

/*!
 * This function is called when a new FBO is needed. This happens on the
 * initial frame. If QQuickFramebufferObject::textureFollowsItemSize is set to true,
 * it is called again every time the dimensions of the item changes.
 *
 * The returned FBO can have any attachment. If the QOpenGLFramebufferObjectFormat
 * indicates that the FBO should be multisampled, the internal implementation
 * of the Renderer will allocate a second FBO and blit the multisampled FBO
 * into the FBO used to display the texture.
 *
 * \note Some hardware has issues with small FBO sizes. \a size takes that into account, so
 * be cautious when overriding the size with a fixed size. A minimal size of 64x64 should
 * always work.
 *
 * \note \a size takes the device pixel ratio into account, meaning that it is
 * already multiplied by the correct scale factor. When moving the window
 * containing the QQuickFramebufferObject item to a screen with different
 * settings, the FBO is automatically recreated and this function is invoked
 * with the correct size.
 */
QOpenGLFramebufferObject *QQuickFramebufferObject::Renderer::createFramebufferObject(const QSize &size)
{
    return new QOpenGLFramebufferObject(size);
}

/*!
 * Call this function when the FBO should be rendered again.
 *
 * This function can be called from render() to force the FBO to be rendered
 * again before the next frame.
 *
 * \note This function should be used from inside the renderer. To update
 * the item on the GUI thread, use QQuickFramebufferObject::update().
 */
void QQuickFramebufferObject::Renderer::update()
{
    if (data)
        ((QSGFramebufferObjectNode *) data)->scheduleRender();
}


#include "qquickframebufferobject.moc"

QT_END_NAMESPACE
