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

#include "qsgengine_p.h"

#include <QtQuick/qsgtexture.h>
#include <private/qsgcontext_p.h>
#include <private/qsgrenderer_p.h>
#include <private/qsgtexture_p.h>

#ifndef QT_NO_OPENGL
# include <QtGui/QOpenGLContext>
# include <private/qsgdefaultrendercontext_p.h>
#endif

QT_BEGIN_NAMESPACE


/*!
    \class QSGEngine
    \brief The QSGEngine class allows low level rendering of a scene graph.
    \inmodule QtQuick
    \since 5.4

    A QSGEngine can be used to render a tree of QSGNode directly on a QWindow
    or QOpenGLFramebufferObject without any integration with QML, QQuickWindow
    or QQuickItem and the convenience that they provide.

    This means that you must handle event propagation, animation timing,
    and node lifetime yourself.

    \note This class is for very low level access to an independent scene graph.
    Most of the time you will instead want to subclass QQuickItem and insert
    your QSGNode in a normal QtQuick scene by overriding QQuickItem::updatePaintNode().

    \sa QSGAbstractRenderer
 */

/*!
    \enum QSGEngine::CreateTextureOption

    The CreateTextureOption enums are used to customize how a texture is wrapped.

    \value TextureHasAlphaChannel The texture has an alpha channel and should
    be drawn using blending.

    \value TextureOwnsGLTexture The texture object owns the texture id and
    will delete the GL texture when the texture object is deleted.

    \value TextureCanUseAtlas The image can be uploaded into a texture atlas.
 */

QSGEnginePrivate::QSGEnginePrivate()
    : sgContext(QSGContext::createDefaultContext())
    , sgRenderContext(sgContext.data()->createRenderContext())
{
}

/*!
    Constructs a new QSGEngine with its \a parent
 */
QSGEngine::QSGEngine(QObject *parent)
    : QObject(*(new QSGEnginePrivate), parent)
{
}

/*!
    Destroys the engine
 */
QSGEngine::~QSGEngine()
{
}

/*!
    Initialize the engine with \a context.

    \warning You have to make sure that you call
    QOpenGLContext::makeCurrent() on \a context before calling this.
 */
void QSGEngine::initialize(QOpenGLContext *context)
{
    Q_D(QSGEngine);
#ifndef QT_NO_OPENGL
    if (context && QOpenGLContext::currentContext() != context) {
        qWarning("WARNING: The context must be current before calling QSGEngine::initialize.");
        return;
    }
#endif
    if (d->sgRenderContext && !d->sgRenderContext->isValid()) {
        d->sgRenderContext->setAttachToGraphicsContext(false);
        d->sgRenderContext->initialize(context);
#ifndef QT_NO_OPENGL
        if (context)
            connect(context, &QOpenGLContext::aboutToBeDestroyed, this, &QSGEngine::invalidate);
#endif
    }
}

/*!
    Invalidate the engine releasing its resources

    You will have to call initialize() and createRenderer() if you
    want to use it again.
 */
void QSGEngine::invalidate()
{
    Q_D(QSGEngine);
    d->sgRenderContext->invalidate();
}

/*!
    Returns a renderer that can be used to render a QSGNode tree

    You call initialize() first with the QOpenGLContext that you
    want to use with this renderer. This will return a null
    renderer otherwise.
 */
QSGAbstractRenderer *QSGEngine::createRenderer() const
{
    Q_D(const QSGEngine);
    if (!d->sgRenderContext->isValid())
        return 0;

    QSGRenderer *renderer = d->sgRenderContext->createRenderer();
    renderer->setCustomRenderMode(qgetenv("QSG_VISUALIZE"));
    return renderer;
}

/*!
    Creates a texture using the data of \a image

    Valid \a options are TextureCanUseAtlas and TextureIsOpaque.

    The caller takes ownership of the texture and the
    texture should only be used with this engine.

    \sa createTextureFromId(), QSGSimpleTextureNode::setOwnsTexture(), QQuickWindow::createTextureFromImage()
 */
QSGTexture *QSGEngine::createTextureFromImage(const QImage &image, CreateTextureOptions options) const
{
    Q_D(const QSGEngine);
     if (!d->sgRenderContext->isValid())
         return 0;
    uint flags = 0;
    if (options & TextureCanUseAtlas)     flags |= QSGRenderContext::CreateTexture_Atlas;
    if (!(options & TextureIsOpaque))     flags |= QSGRenderContext::CreateTexture_Alpha;
    return d->sgRenderContext->createTexture(image, flags);
}

/*!
    Creates a texture object that wraps the GL texture \a id uploaded with \a size

    Valid \a options are TextureHasAlphaChannel and TextureOwnsGLTexture

    The caller takes ownership of the texture object and the
    texture should only be used with this engine.

    \sa createTextureFromImage(), QSGSimpleTextureNode::setOwnsTexture(), QQuickWindow::createTextureFromId()
 */
QSGTexture *QSGEngine::createTextureFromId(uint id, const QSize &size, CreateTextureOptions options) const
{
    Q_D(const QSGEngine);
    if (d->sgRenderContext->isValid()) {
        QSGPlainTexture *texture = new QSGPlainTexture();
        texture->setTextureId(id);
        texture->setHasAlphaChannel(options & TextureHasAlphaChannel);
        texture->setOwnsTexture(options & TextureOwnsGLTexture);
        texture->setTextureSize(size);
        return texture;
    }
    return 0;
}

/*!
    Returns the current renderer interface if there is one. Otherwise null is returned.

    \sa QSGRenderNode, QSGRendererInterface
    \since 5.8
 */
QSGRendererInterface *QSGEngine::rendererInterface() const
{
    Q_D(const QSGEngine);
    return d->sgRenderContext->isValid()
            ? d->sgRenderContext->sceneGraphContext()->rendererInterface(d->sgRenderContext.data())
            : nullptr;
}

/*!
    Creates a simple rectangle node. When the scenegraph is not initialized, the return value is null.

    This is cross-backend alternative to constructing a QSGSimpleRectNode directly.

    \since 5.8
    \sa QSGRectangleNode
 */
QSGRectangleNode *QSGEngine::createRectangleNode() const
{
    Q_D(const QSGEngine);
    return d->sgRenderContext->isValid() ? d->sgRenderContext->sceneGraphContext()->createRectangleNode() : nullptr;
}

/*!
    Creates a simple image node. When the scenegraph is not initialized, the return value is null.

    This is cross-backend alternative to constructing a QSGSimpleTextureNode directly.

    \since 5.8
    \sa QSGImageNode
 */

QSGImageNode *QSGEngine::createImageNode() const
{
    Q_D(const QSGEngine);
    return d->sgRenderContext->isValid() ? d->sgRenderContext->sceneGraphContext()->createImageNode() : nullptr;
}

/*!
    Creates a nine patch node. When the scenegraph is not initialized, the return value is null.

    \since 5.8
 */

QSGNinePatchNode *QSGEngine::createNinePatchNode() const
{
    Q_D(const QSGEngine);
    return d->sgRenderContext->isValid() ? d->sgRenderContext->sceneGraphContext()->createNinePatchNode() : nullptr;
}

QT_END_NAMESPACE
