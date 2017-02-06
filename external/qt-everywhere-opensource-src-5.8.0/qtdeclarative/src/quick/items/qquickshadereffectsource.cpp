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

#include "qquickshadereffectsource_p.h"

#include "qquickitem_p.h"
#include "qquickwindow_p.h"
#include <private/qsgadaptationlayer_p.h>
#include <QtQuick/private/qsgrenderer_p.h>
#include <qsgsimplerectnode.h>

#include "qmath.h"
#include <QtQuick/private/qsgtexture_p.h>
#include <QtCore/QRunnable>

QT_BEGIN_NAMESPACE

class QQuickShaderEffectSourceTextureProvider : public QSGTextureProvider
{
    Q_OBJECT
public:
    QQuickShaderEffectSourceTextureProvider()
        : sourceTexture(0)
        , mipmapFiltering(QSGTexture::None)
        , filtering(QSGTexture::Nearest)
        , horizontalWrap(QSGTexture::ClampToEdge)
        , verticalWrap(QSGTexture::ClampToEdge)
    {
    }

    QSGTexture *texture() const {
        sourceTexture->setMipmapFiltering(mipmapFiltering);
        sourceTexture->setFiltering(filtering);
        sourceTexture->setHorizontalWrapMode(horizontalWrap);
        sourceTexture->setVerticalWrapMode(verticalWrap);
        return sourceTexture;
    }

    QSGLayer *sourceTexture;

    QSGTexture::Filtering mipmapFiltering;
    QSGTexture::Filtering filtering;
    QSGTexture::WrapMode horizontalWrap;
    QSGTexture::WrapMode verticalWrap;
};

class QQuickShaderEffectSourceCleanup : public QRunnable
{
public:
    QQuickShaderEffectSourceCleanup(QSGLayer *t, QQuickShaderEffectSourceTextureProvider *p)
        : texture(t)
        , provider(p)
    {}
    void run() Q_DECL_OVERRIDE {
        delete texture;
        delete provider;
    }
    QSGLayer *texture;
    QQuickShaderEffectSourceTextureProvider *provider;
};

/*!
    \qmltype ShaderEffectSource
    \instantiates QQuickShaderEffectSource
    \inqmlmodule QtQuick
    \since 5.0
    \inherits Item
    \ingroup qtquick-effects
    \brief Renders a \l {Qt Quick} item into a texture and displays it

    The ShaderEffectSource type renders \l sourceItem into a texture and
    displays it in the scene. \l sourceItem is drawn into the texture as though
    it was a fully opaque root item. Thus \l sourceItem itself can be
    invisible, but still appear in the texture.

    ShaderEffectSource can be used as:
    \list
    \li a texture source in a \l ShaderEffect.
       This allows you to apply custom shader effects to any \l {Qt Quick} item.
    \li a cache for a complex item.
       The complex item can be rendered once into the texture, which can
       then be animated freely without the need to render the complex item
       again every frame.
    \li an opacity layer.
       ShaderEffectSource allows you to apply an opacity to items as a group
       rather than each item individually.
    \endlist

    \table
    \row
    \li \image declarative-shadereffectsource.png
    \li \qml
        import QtQuick 2.0

        Rectangle {
            width: 200
            height: 100
            gradient: Gradient {
                GradientStop { position: 0; color: "white" }
                GradientStop { position: 1; color: "black" }
            }
            Row {
                opacity: 0.5
                Item {
                    id: foo
                    width: 100; height: 100
                    Rectangle { x: 5; y: 5; width: 60; height: 60; color: "red" }
                    Rectangle { x: 20; y: 20; width: 60; height: 60; color: "orange" }
                    Rectangle { x: 35; y: 35; width: 60; height: 60; color: "yellow" }
                }
                ShaderEffectSource {
                    width: 100; height: 100
                    sourceItem: foo
                }
            }
        }
        \endqml
    \endtable

    The ShaderEffectSource type does not redirect any mouse or keyboard
    input to \l sourceItem. If you hide the \l sourceItem by setting
    \l{Item::visible}{visible} to false or \l{Item::opacity}{opacity} to zero,
    it will no longer react to input. In cases where the ShaderEffectSource is
    meant to replace the \l sourceItem, you typically want to hide the
    \l sourceItem while still handling input. For this, you can use
    the \l hideSource property.

    \note If \l sourceItem is a \l Rectangle with border, by default half the
    border width falls outside the texture. To get the whole border, you can
    extend the \l sourceRect.

    \note The ShaderEffectSource relies on FBO multisampling support
    to antialias edges. If the underlying hardware does not support this,
    which is the case for most embedded graphics chips, edges rendered
    inside a ShaderEffectSource will not be antialiased. One way to remedy
    this is to double the size of the effect source and render it with
    \c {smooth: true} (this is the default value of smooth).
    This will be equivalent to 4x multisampling, at the cost of lower performance
    and higher memory use.

    \warning In most cases, using a ShaderEffectSource will decrease
    performance, and in all cases, it will increase video memory usage.
    Rendering through a ShaderEffectSource might also lead to lower quality
    since some OpenGL implementations support multisampled backbuffer,
    but not multisampled framebuffer objects.
*/

QQuickShaderEffectSource::QQuickShaderEffectSource(QQuickItem *parent)
    : QQuickItem(parent)
    , m_provider(0)
    , m_texture(0)
    , m_wrapMode(ClampToEdge)
    , m_sourceItem(0)
    , m_textureSize(0, 0)
    , m_format(RGBA)
    , m_live(true)
    , m_hideSource(false)
    , m_mipmap(false)
    , m_recursive(false)
    , m_grab(true)
    , m_textureMirroring(MirrorVertically)
{
    setFlag(ItemHasContents);
}

QQuickShaderEffectSource::~QQuickShaderEffectSource()
{
    if (window()) {
        window()->scheduleRenderJob(new QQuickShaderEffectSourceCleanup(m_texture, m_provider),
                                    QQuickWindow::AfterSynchronizingStage);
    } else {
        // If we don't have a window, these should already have been
        // released in invalidateSG or in releaseResrouces()
        Q_ASSERT(!m_texture);
        Q_ASSERT(!m_provider);
    }

    if (m_sourceItem) {
        QQuickItemPrivate *sd = QQuickItemPrivate::get(m_sourceItem);
        sd->removeItemChangeListener(this, QQuickItemPrivate::Geometry);
        sd->derefFromEffectItem(m_hideSource);
        if (window())
            sd->derefWindow();
    }
}

void QQuickShaderEffectSource::ensureTexture()
{
    if (m_texture)
        return;

    Q_ASSERT_X(QQuickItemPrivate::get(this)->window
               && QQuickItemPrivate::get(this)->sceneGraphRenderContext()
               && QThread::currentThread() == QQuickItemPrivate::get(this)->sceneGraphRenderContext()->thread(),
               "QQuickShaderEffectSource::ensureTexture",
               "Cannot be used outside the rendering thread");

    QSGRenderContext *rc = QQuickItemPrivate::get(this)->sceneGraphRenderContext();
    m_texture = rc->sceneGraphContext()->createLayer(rc);
    connect(QQuickItemPrivate::get(this)->window, SIGNAL(sceneGraphInvalidated()), m_texture, SLOT(invalidated()), Qt::DirectConnection);
    connect(m_texture, SIGNAL(updateRequested()), this, SLOT(update()));
    connect(m_texture, SIGNAL(scheduledUpdateCompleted()), this, SIGNAL(scheduledUpdateCompleted()));
}

static void get_wrap_mode(QQuickShaderEffectSource::WrapMode mode, QSGTexture::WrapMode *hWrap, QSGTexture::WrapMode *vWrap);

QSGTextureProvider *QQuickShaderEffectSource::textureProvider() const
{
    const QQuickItemPrivate *d = QQuickItemPrivate::get(this);
    if (!d->window || !d->sceneGraphRenderContext() || QThread::currentThread() != d->sceneGraphRenderContext()->thread()) {
        qWarning("QQuickShaderEffectSource::textureProvider: can only be queried on the rendering thread of an exposed window");
        return 0;
    }

    if (!m_provider) {
        const_cast<QQuickShaderEffectSource *>(this)->m_provider = new QQuickShaderEffectSourceTextureProvider();
        const_cast<QQuickShaderEffectSource *>(this)->ensureTexture();
        connect(m_texture, SIGNAL(updateRequested()), m_provider, SIGNAL(textureChanged()));

        get_wrap_mode(m_wrapMode, &m_provider->horizontalWrap, &m_provider->verticalWrap);
        m_provider->mipmapFiltering = mipmap() ? QSGTexture::Linear : QSGTexture::None;
        m_provider->filtering = smooth() ? QSGTexture::Linear : QSGTexture::Nearest;
        m_provider->sourceTexture = m_texture;
    }
    return m_provider;
}

/*!
    \qmlproperty enumeration QtQuick::ShaderEffectSource::wrapMode

    This property defines the OpenGL wrap modes associated with the texture.
    Modifying this property makes most sense when the item is used as a
    source texture of a \l ShaderEffect.

    The default value is \c{ShaderEffectSource.ClampToEdge}.

    \list
    \li ShaderEffectSource.ClampToEdge - GL_CLAMP_TO_EDGE both horizontally and vertically
    \li ShaderEffectSource.RepeatHorizontally - GL_REPEAT horizontally, GL_CLAMP_TO_EDGE vertically
    \li ShaderEffectSource.RepeatVertically - GL_CLAMP_TO_EDGE horizontally, GL_REPEAT vertically
    \li ShaderEffectSource.Repeat - GL_REPEAT both horizontally and vertically
    \endlist

    \note Some OpenGL ES 2 implementations do not support the GL_REPEAT
    wrap mode with non-power-of-two textures.
*/

QQuickShaderEffectSource::WrapMode QQuickShaderEffectSource::wrapMode() const
{
    return m_wrapMode;
}

void QQuickShaderEffectSource::setWrapMode(WrapMode mode)
{
    if (mode == m_wrapMode)
        return;
    m_wrapMode = mode;
    update();
    emit wrapModeChanged();
}

/*!
    \qmlproperty Item QtQuick::ShaderEffectSource::sourceItem

    This property holds the item to be rendered into the texture.
    Setting this to null while \l live is true, will release the texture
    resources.
*/

QQuickItem *QQuickShaderEffectSource::sourceItem() const
{
    return m_sourceItem;
}

void QQuickShaderEffectSource::itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &)
{
    Q_ASSERT(item == m_sourceItem);
    Q_UNUSED(item);
    if (change.sizeChange())
        update();
}

void QQuickShaderEffectSource::setSourceItem(QQuickItem *item)
{
    if (item == m_sourceItem)
        return;
    if (m_sourceItem) {
        QQuickItemPrivate *d = QQuickItemPrivate::get(m_sourceItem);
        d->derefFromEffectItem(m_hideSource);
        d->removeItemChangeListener(this, QQuickItemPrivate::Geometry);
        disconnect(m_sourceItem, SIGNAL(destroyed(QObject*)), this, SLOT(sourceItemDestroyed(QObject*)));
        if (window())
            d->derefWindow();
    }

    m_sourceItem = item;

    if (m_sourceItem) {
        if (window() == m_sourceItem->window()
                || (window() == 0 && m_sourceItem->window())
                || (m_sourceItem->window() == 0 && window())) {
            QQuickItemPrivate *d = QQuickItemPrivate::get(item);
            // 'item' needs a window to get a scene graph node. It usually gets one through its
            // parent, but if the source item is "inline" rather than a reference -- i.e.
            // "sourceItem: Item { }" instead of "sourceItem: foo" -- it will not get a parent.
            // In those cases, 'item' should get the window from 'this'.
            if (window())
                d->refWindow(window());
            else if (m_sourceItem->window())
                d->refWindow(m_sourceItem->window());
            d->refFromEffectItem(m_hideSource);
            d->addItemChangeListener(this, QQuickItemPrivate::Geometry);
            connect(m_sourceItem, SIGNAL(destroyed(QObject*)), this, SLOT(sourceItemDestroyed(QObject*)));
        } else {
            qWarning("ShaderEffectSource: sourceItem and ShaderEffectSource must both be children of the same window.");
            m_sourceItem = 0;
        }
    }
    update();
    emit sourceItemChanged();
}

void QQuickShaderEffectSource::sourceItemDestroyed(QObject *item)
{
    Q_ASSERT(item == m_sourceItem);
    Q_UNUSED(item);
    m_sourceItem = 0;
    update();
    emit sourceItemChanged();
}


/*!
    \qmlproperty rect QtQuick::ShaderEffectSource::sourceRect

    This property defines which rectangular area of the \l sourceItem to
    render into the texture. The source rectangle can be larger than
    \l sourceItem itself. If the rectangle is null, which is the default,
    the whole \l sourceItem is rendered to texture.
*/

QRectF QQuickShaderEffectSource::sourceRect() const
{
    return m_sourceRect;
}

void QQuickShaderEffectSource::setSourceRect(const QRectF &rect)
{
    if (rect == m_sourceRect)
        return;
    m_sourceRect = rect;
    update();
    emit sourceRectChanged();
}

/*!
    \qmlproperty size QtQuick::ShaderEffectSource::textureSize

    This property holds the requested size of the texture. If it is empty,
    which is the default, the size of the source rectangle is used.

    \note Some platforms have a limit on how small framebuffer objects can be,
    which means the actual texture size might be larger than the requested
    size.
*/

QSize QQuickShaderEffectSource::textureSize() const
{
    return m_textureSize;
}

void QQuickShaderEffectSource::setTextureSize(const QSize &size)
{
    if (size == m_textureSize)
        return;
    m_textureSize = size;
    update();
    emit textureSizeChanged();
}

/*!
    \qmlproperty enumeration QtQuick::ShaderEffectSource::format

    This property defines the internal OpenGL format of the texture.
    Modifying this property makes most sense when the item is used as a
    source texture of a \l ShaderEffect. Depending on the OpenGL
    implementation, this property might allow you to save some texture memory.

    \list
    \li ShaderEffectSource.Alpha - GL_ALPHA
    \li ShaderEffectSource.RGB - GL_RGB
    \li ShaderEffectSource.RGBA - GL_RGBA
    \endlist

    \note Some OpenGL implementations do not support the GL_ALPHA format.
*/

QQuickShaderEffectSource::Format QQuickShaderEffectSource::format() const
{
    return m_format;
}

void QQuickShaderEffectSource::setFormat(QQuickShaderEffectSource::Format format)
{
    if (format == m_format)
        return;
    m_format = format;
    update();
    emit formatChanged();
}

/*!
    \qmlproperty bool QtQuick::ShaderEffectSource::live

    If this property is true, the texture is updated whenever the
    \l sourceItem updates. Otherwise, it will be a frozen image, even if
    \l sourceItem is assigned a new item. The property is true by default.
*/

bool QQuickShaderEffectSource::live() const
{
    return m_live;
}

void QQuickShaderEffectSource::setLive(bool live)
{
    if (live == m_live)
        return;
    m_live = live;
    update();
    emit liveChanged();
}

/*!
    \qmlproperty bool QtQuick::ShaderEffectSource::hideSource

    If this property is true, the \l sourceItem is hidden, though it will still
    be rendered into the texture. As opposed to hiding the \l sourceItem by
    setting \l{Item::visible}{visible} to false, setting this property to true
    will not prevent mouse or keyboard input from reaching \l sourceItem.
    The property is useful when the ShaderEffectSource is anchored on top of,
    and meant to replace the \l sourceItem.
*/

bool QQuickShaderEffectSource::hideSource() const
{
    return m_hideSource;
}

void QQuickShaderEffectSource::setHideSource(bool hide)
{
    if (hide == m_hideSource)
        return;
    if (m_sourceItem) {
        QQuickItemPrivate::get(m_sourceItem)->refFromEffectItem(hide);
        QQuickItemPrivate::get(m_sourceItem)->derefFromEffectItem(m_hideSource);
    }
    m_hideSource = hide;
    update();
    emit hideSourceChanged();
}

/*!
    \qmlproperty bool QtQuick::ShaderEffectSource::mipmap

    If this property is true, mipmaps are generated for the texture.

    \note Some OpenGL ES 2 implementations do not support mipmapping of
    non-power-of-two textures.
*/

bool QQuickShaderEffectSource::mipmap() const
{
    return m_mipmap;
}

void QQuickShaderEffectSource::setMipmap(bool enabled)
{
    if (enabled == m_mipmap)
        return;
    m_mipmap = enabled;
    update();
    emit mipmapChanged();
}

/*!
    \qmlproperty bool QtQuick::ShaderEffectSource::recursive

    Set this property to true if the ShaderEffectSource has a dependency on
    itself. ShaderEffectSources form a dependency chain, where one
    ShaderEffectSource can be part of the \l sourceItem of another.
    If there is a loop in this chain, a ShaderEffectSource could end up trying
    to render into the same texture it is using as source, which is not allowed
    by OpenGL. When this property is set to true, an extra texture is allocated
    so that ShaderEffectSource can keep a copy of the texture from the previous
    frame. It can then render into one texture and use the texture from the
    previous frame as source.

    Setting both this property and \l live to true will cause the scene graph
    to render continuously. Since the ShaderEffectSource depends on itself,
    updating it means that it immediately becomes dirty again.
*/

bool QQuickShaderEffectSource::recursive() const
{
    return m_recursive;
}

void QQuickShaderEffectSource::setRecursive(bool enabled)
{
    if (enabled == m_recursive)
        return;
    m_recursive = enabled;
    emit recursiveChanged();
}

/*!
    \qmlproperty enumeration QtQuick::ShaderEffectSource::textureMirroring
    \since 5.6

    This property defines how the generated OpenGL texture should be mirrored.
    The default value is \c{ShaderEffectSource.MirrorVertically}.
    Custom mirroring can be useful if the generated texture is directly accessed by custom shaders,
    such as those specified by ShaderEffect. Mirroring has no effect on the UI representation of
    the ShaderEffectSource item itself.

    \list
    \li ShaderEffectSource.NoMirroring - No mirroring
    \li ShaderEffectSource.MirrorHorizontally - The generated texture is flipped along X-axis.
    \li ShaderEffectSource.MirrorVertically - The generated texture is flipped along Y-axis.
    \endlist
*/

QQuickShaderEffectSource::TextureMirroring QQuickShaderEffectSource::textureMirroring() const
{
    return QQuickShaderEffectSource::TextureMirroring(m_textureMirroring);
}

void QQuickShaderEffectSource::setTextureMirroring(TextureMirroring mirroring)
{
    if (mirroring == QQuickShaderEffectSource::TextureMirroring(m_textureMirroring))
        return;
    m_textureMirroring = mirroring;
    update();
    emit textureMirroringChanged();
}

/*!
    \qmlmethod QtQuick::ShaderEffectSource::scheduleUpdate()

    Schedules a re-rendering of the texture for the next frame.
    Use this to update the texture when \l live is false.
*/

void QQuickShaderEffectSource::scheduleUpdate()
{
    if (m_grab)
        return;
    m_grab = true;
    update();
}

static void get_wrap_mode(QQuickShaderEffectSource::WrapMode mode, QSGTexture::WrapMode *hWrap, QSGTexture::WrapMode *vWrap)
{
    switch (mode) {
    case QQuickShaderEffectSource::RepeatHorizontally:
        *hWrap = QSGTexture::Repeat;
        *vWrap = QSGTexture::ClampToEdge;
        break;
    case QQuickShaderEffectSource::RepeatVertically:
        *vWrap = QSGTexture::Repeat;
        *hWrap = QSGTexture::ClampToEdge;
        break;
    case QQuickShaderEffectSource::Repeat:
        *hWrap = *vWrap = QSGTexture::Repeat;
        break;
    default:
        // QQuickShaderEffectSource::ClampToEdge
        *hWrap = *vWrap = QSGTexture::ClampToEdge;
        break;
    }
}


void QQuickShaderEffectSource::releaseResources()
{
    if (m_texture || m_provider) {
        window()->scheduleRenderJob(new QQuickShaderEffectSourceCleanup(m_texture, m_provider),
                                    QQuickWindow::AfterSynchronizingStage);
        m_texture = 0;
        m_provider = 0;
    }
}

class QQuickShaderSourceAttachedNode : public QObject, public QSGNode
{
    Q_OBJECT
public:
    Q_SLOT void markTextureDirty() {
        QSGNode *pn = QSGNode::parent();
        if (pn) {
            Q_ASSERT(pn->type() == QSGNode::GeometryNodeType);
            pn->markDirty(DirtyMaterial);
        }
    }
};

QSGNode *QQuickShaderEffectSource::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (!m_sourceItem || m_sourceItem->width() <= 0 || m_sourceItem->height() <= 0) {
        if (m_texture)
            m_texture->setItem(0);
        delete oldNode;
        return 0;
    }

    ensureTexture();

    m_texture->setLive(m_live);
    m_texture->setItem(QQuickItemPrivate::get(m_sourceItem)->itemNode());
    QRectF sourceRect = m_sourceRect.width() == 0 || m_sourceRect.height() == 0
                      ? QRectF(0, 0, m_sourceItem->width(), m_sourceItem->height())
                      : m_sourceRect;
    m_texture->setRect(sourceRect);
    QSize textureSize = m_textureSize.isEmpty()
                      ? QSize(qCeil(qAbs(sourceRect.width())), qCeil(qAbs(sourceRect.height())))
                      : m_textureSize;
    Q_ASSERT(!textureSize.isEmpty());

    QQuickItemPrivate *d = static_cast<QQuickItemPrivate *>(QObjectPrivate::get(this));

    // Crate large textures on high-dpi displays.
    if (sourceItem())
        textureSize *= d->window->effectiveDevicePixelRatio();

    const QSize minTextureSize = d->sceneGraphContext()->minimumFBOSize();
    // Keep power-of-two by doubling the size.
    while (textureSize.width() < minTextureSize.width())
        textureSize.rwidth() *= 2;
    while (textureSize.height() < minTextureSize.height())
        textureSize.rheight() *= 2;

    m_texture->setDevicePixelRatio(d->window->effectiveDevicePixelRatio());
    m_texture->setSize(textureSize);
    m_texture->setRecursive(m_recursive);
    m_texture->setFormat(m_format);
    m_texture->setHasMipmaps(m_mipmap);
    m_texture->setMirrorHorizontal(m_textureMirroring & MirrorHorizontally);
    m_texture->setMirrorVertical(m_textureMirroring & MirrorVertically);

    if (m_grab)
        m_texture->scheduleUpdate();
    m_grab = false;

    QSGTexture::Filtering filtering = QQuickItemPrivate::get(this)->smooth
                                            ? QSGTexture::Linear
                                            : QSGTexture::Nearest;
    QSGTexture::Filtering mmFiltering = m_mipmap ? filtering : QSGTexture::None;
    QSGTexture::WrapMode hWrap, vWrap;
    get_wrap_mode(m_wrapMode, &hWrap, &vWrap);

    if (m_provider) {
        m_provider->mipmapFiltering = mmFiltering;
        m_provider->filtering = filtering;
        m_provider->horizontalWrap = hWrap;
        m_provider->verticalWrap = vWrap;
    }

    // Don't create the paint node if we're not spanning any area
    if (width() <= 0 || height() <= 0) {
        delete oldNode;
        return 0;
    }

    QSGInternalImageNode *node = static_cast<QSGInternalImageNode *>(oldNode);
    if (!node) {
        node = d->sceneGraphContext()->createInternalImageNode();
        node->setFlag(QSGNode::UsePreprocess);
        node->setTexture(m_texture);
        QQuickShaderSourceAttachedNode *attached = new QQuickShaderSourceAttachedNode;
        node->appendChildNode(attached);
        connect(m_texture, SIGNAL(updateRequested()), attached, SLOT(markTextureDirty()));
    }

    // If live and recursive, update continuously.
    if (m_live && m_recursive)
        node->markDirty(QSGNode::DirtyMaterial);

    node->setMipmapFiltering(mmFiltering);
    node->setFiltering(filtering);
    node->setHorizontalWrapMode(hWrap);
    node->setVerticalWrapMode(vWrap);
    node->setTargetRect(QRectF(0, 0, width(), height()));
    node->setInnerTargetRect(QRectF(0, 0, width(), height()));
    node->update();

    return node;
}

void QQuickShaderEffectSource::invalidateSceneGraph()
{
    if (m_texture)
        delete m_texture;
    if (m_provider)
        delete m_provider;
    m_texture = 0;
    m_provider = 0;
}

void QQuickShaderEffectSource::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == QQuickItem::ItemSceneChange && m_sourceItem) {
        // See comment in QQuickShaderEffectSource::setSourceItem().
        if (value.window)
            QQuickItemPrivate::get(m_sourceItem)->refWindow(value.window);
        else
            QQuickItemPrivate::get(m_sourceItem)->derefWindow();
    }
    QQuickItem::itemChange(change, value);
}

#include "qquickshadereffectsource.moc"

QT_END_NAMESPACE
