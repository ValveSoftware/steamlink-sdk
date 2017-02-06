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

#include "qquickcontext2dtexture_p.h"
#include "qquickcontext2dtile_p.h"
#include "qquickcanvasitem_p.h"
#include <private/qquickitem_p.h>
#include <QtQuick/private/qsgtexture_p.h>
#include "qquickcontext2dcommandbuffer_p.h"
#include <QOpenGLPaintDevice>
#ifndef QT_NO_OPENGL
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions>
#endif
#include <QtCore/QThread>
#include <QtGui/QGuiApplication>

QT_BEGIN_NAMESPACE
#ifndef QT_NO_OPENGL
#define QT_MINIMUM_FBO_SIZE 64

static inline int qt_next_power_of_two(int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    ++v;
    return v;
}

struct GLAcquireContext {
    GLAcquireContext(QOpenGLContext *c, QSurface *s):ctx(c) {
        if (ctx) {
            Q_ASSERT(s);
            if (!ctx->isValid())
                ctx->create();

            if (!ctx->isValid())
                qWarning() << "Unable to create GL context";
            else if (!ctx->makeCurrent(s))
                qWarning() << "Can't make current GL context";
        }
    }
    ~GLAcquireContext() {
        if (ctx)
            ctx->doneCurrent();
    }
    QOpenGLContext *ctx;
};
#endif
QQuickContext2DTexture::QQuickContext2DTexture()
    : m_context(0)
#ifndef QT_NO_OPENGL
    , m_gl(0)
#endif
    , m_surface(0)
    , m_item(0)
    , m_canvasWindowChanged(false)
    , m_dirtyTexture(false)
    , m_smooth(true)
    , m_antialiasing(false)
    , m_tiledCanvas(false)
    , m_painting(false)
{
}

QQuickContext2DTexture::~QQuickContext2DTexture()
{
   clearTiles();
}

void QQuickContext2DTexture::markDirtyTexture()
{
    if (m_onCustomThread)
        m_mutex.lock();
    m_dirtyTexture = true;
    emit textureChanged();
    if (m_onCustomThread)
        m_mutex.unlock();
}

bool QQuickContext2DTexture::setCanvasSize(const QSize &size)
{
    if (m_canvasSize != size) {
        m_canvasSize = size;
        return true;
    }
    return false;
}

bool QQuickContext2DTexture::setTileSize(const QSize &size)
{
    if (m_tileSize != size) {
        m_tileSize = size;
        return true;
    }
    return false;
}

void QQuickContext2DTexture::setSmooth(bool smooth)
{
    m_smooth = smooth;
}

void QQuickContext2DTexture::setAntialiasing(bool antialiasing)
{
    m_antialiasing = antialiasing;
}

void QQuickContext2DTexture::setItem(QQuickCanvasItem* item)
{
    m_item = item;
    if (m_item) {
        m_context = (QQuickContext2D*) item->rawContext(); // FIXME
        m_state = m_context->state;
    } else {
        m_context = 0;
    }
}

bool QQuickContext2DTexture::setCanvasWindow(const QRect& r)
{
    if (m_canvasWindow != r) {
        m_canvasWindow = r;
        m_canvasWindowChanged = true;
        return true;
    }
    return false;
}

bool QQuickContext2DTexture::setDirtyRect(const QRect &r)
{
    bool doDirty = false;
    if (m_tiledCanvas) {
        for (QQuickContext2DTile* t : qAsConst(m_tiles)) {
            bool dirty = t->rect().intersected(r).isValid();
            t->markDirty(dirty);
            if (dirty)
                doDirty = true;
        }
    } else {
        doDirty = m_canvasWindow.intersected(r).isValid();
    }
    return doDirty;
}

void QQuickContext2DTexture::canvasChanged(const QSize& canvasSize, const QSize& tileSize, const QRect& canvasWindow, const QRect& dirtyRect, bool smooth, bool antialiasing)
{
    QSize ts = tileSize;
    if (ts.width() > canvasSize.width())
        ts.setWidth(canvasSize.width());

    if (ts.height() > canvasSize.height())
        ts.setHeight(canvasSize.height());

    setCanvasSize(canvasSize);
    setTileSize(ts);
    setCanvasWindow(canvasWindow);

    if (canvasSize == canvasWindow.size()) {
        m_tiledCanvas = false;
    } else {
        m_tiledCanvas = true;
    }

    if (dirtyRect.isValid())
        setDirtyRect(dirtyRect);

    setSmooth(smooth);
    setAntialiasing(antialiasing);
}

void QQuickContext2DTexture::paintWithoutTiles(QQuickContext2DCommandBuffer *ccb)
{
    if (!ccb || ccb->isEmpty())
        return;

    QPaintDevice* device = beginPainting();
    if (!device) {
        endPainting();
        return;
    }

    QPainter p;
    p.begin(device);
    if (m_antialiasing)
        p.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing | QPainter::TextAntialiasing, true);
    else
        p.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing | QPainter::TextAntialiasing, false);

    if (m_smooth)
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    else
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);

    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    ccb->replay(&p, m_state, scaleFactor());
    endPainting();
    markDirtyTexture();
}

bool QQuickContext2DTexture::canvasDestroyed()
{
    return m_item == 0;
}

void QQuickContext2DTexture::paint(QQuickContext2DCommandBuffer *ccb)
{
    QQuickContext2D::mutex.lock();
    if (canvasDestroyed()) {
        delete ccb;
        QQuickContext2D::mutex.unlock();
        return;
    }
    QQuickContext2D::mutex.unlock();
#ifndef QT_NO_OPENGL
    GLAcquireContext currentContext(m_gl, m_surface);
#endif
    if (!m_tiledCanvas) {
        paintWithoutTiles(ccb);
        delete ccb;
        return;
    }

    QRect tiledRegion = createTiles(m_canvasWindow.intersected(QRect(QPoint(0, 0), m_canvasSize)));
    if (!tiledRegion.isEmpty()) {
        QRect dirtyRect;
        for (QQuickContext2DTile* tile : qAsConst(m_tiles)) {
            if (tile->dirty()) {
                if (dirtyRect.isEmpty())
                    dirtyRect = tile->rect();
                else
                    dirtyRect |= tile->rect();
            }
        }

        if (beginPainting()) {
            QQuickContext2D::State oldState = m_state;
            for (QQuickContext2DTile* tile : qAsConst(m_tiles)) {
                if (tile->dirty()) {
                    ccb->replay(tile->createPainter(m_smooth, m_antialiasing), oldState, scaleFactor());
                    tile->drawFinished();
                    tile->markDirty(false);
                }
                compositeTile(tile);
            }
            endPainting();
            m_state = oldState;
            markDirtyTexture();
        }
    }
    delete ccb;
}

QRect QQuickContext2DTexture::tiledRect(const QRectF& window, const QSize& tileSize)
{
    if (window.isEmpty())
        return QRect();

    const int tw = tileSize.width();
    const int th = tileSize.height();
    const int h1 = window.left() / tw;
    const int v1 = window.top() / th;

    const int htiles = ((window.right() - h1 * tw) + tw - 1)/tw;
    const int vtiles = ((window.bottom() - v1 * th) + th - 1)/th;

    return QRect(h1 * tw, v1 * th, htiles * tw, vtiles * th);
}

QRect QQuickContext2DTexture::createTiles(const QRect& window)
{
    QList<QQuickContext2DTile*> oldTiles = m_tiles;
    m_tiles.clear();

    if (window.isEmpty()) {
        return QRect();
    }

    QRect r = tiledRect(window, adjustedTileSize(m_tileSize));

    const int tw = m_tileSize.width();
    const int th = m_tileSize.height();
    const int h1 = window.left() / tw;
    const int v1 = window.top() / th;


    const int htiles = r.width() / tw;
    const int vtiles = r.height() / th;

    for (int yy = 0; yy < vtiles; ++yy) {
        for (int xx = 0; xx < htiles; ++xx) {
            int ht = xx + h1;
            int vt = yy + v1;

            QQuickContext2DTile* tile = 0;

            QPoint pos(ht * tw, vt * th);
            QRect rect(pos, m_tileSize);

            for (int i = 0; i < oldTiles.size(); i++) {
                if (oldTiles[i]->rect() == rect) {
                    tile = oldTiles.takeAt(i);
                    break;
                }
            }

            if (!tile)
                tile = createTile();

            tile->setRect(rect);
            m_tiles.append(tile);
        }
    }

    qDeleteAll(oldTiles);

    return r;
}

void QQuickContext2DTexture::clearTiles()
{
    qDeleteAll(m_tiles);
    m_tiles.clear();
}

QSize QQuickContext2DTexture::adjustedTileSize(const QSize &ts)
{
    return ts;
}

bool QQuickContext2DTexture::event(QEvent *e)
{
    if ((int) e->type() == QEvent::User + 1) {
        PaintEvent *pe = static_cast<PaintEvent *>(e);
        paint(pe->buffer);
        return true;
    } else if ((int) e->type() == QEvent::User + 2) {
        CanvasChangeEvent *ce = static_cast<CanvasChangeEvent *>(e);
        canvasChanged(ce->canvasSize, ce->tileSize, ce->canvasWindow, ce->dirtyRect, ce->smooth, ce->antialiasing);
        return true;
    }
    return QObject::event(e);
}
#ifndef QT_NO_OPENGL
static inline QSize npotAdjustedSize(const QSize &size)
{
    static bool checked = false;
    static bool npotSupported = false;

    if (!checked) {
        npotSupported = QOpenGLContext::currentContext()->functions()->hasOpenGLFeature(QOpenGLFunctions::NPOTTextures);
        checked = true;
    }

    if (npotSupported) {
        return QSize(qMax(QT_MINIMUM_FBO_SIZE, size.width()),
                     qMax(QT_MINIMUM_FBO_SIZE, size.height()));
    }

    return QSize(qMax(QT_MINIMUM_FBO_SIZE, qt_next_power_of_two(size.width())),
                       qMax(QT_MINIMUM_FBO_SIZE, qt_next_power_of_two(size.height())));
}

QQuickContext2DFBOTexture::QQuickContext2DFBOTexture()
    : QQuickContext2DTexture()
    , m_fbo(0)
    , m_multisampledFbo(0)
    , m_paint_device(0)
{
    m_displayTextures[0] = 0;
    m_displayTextures[1] = 0;
    m_displayTexture = -1;
}

QQuickContext2DFBOTexture::~QQuickContext2DFBOTexture()
{
    if (m_multisampledFbo)
        m_multisampledFbo->release();
    else if (m_fbo)
        m_fbo->release();

    delete m_fbo;
    delete m_multisampledFbo;
    delete m_paint_device;

    if (QOpenGLContext::currentContext())
        QOpenGLContext::currentContext()->functions()->glDeleteTextures(2, m_displayTextures);
}

QVector2D QQuickContext2DFBOTexture::scaleFactor() const
{
    if (!m_fbo)
        return QVector2D(1, 1);
    return QVector2D(m_fbo->width() / m_fboSize.width(),
                     m_fbo->height() / m_fboSize.height());
}

QSGTexture *QQuickContext2DFBOTexture::textureForNextFrame(QSGTexture *lastTexture, QQuickWindow *)
{
    QSGPlainTexture *texture = static_cast<QSGPlainTexture *>(lastTexture);

    if (m_onCustomThread)
        m_mutex.lock();

    if (m_fbo) {
        if (!texture) {
            texture = new QSGPlainTexture();
            texture->setHasAlphaChannel(true);
            texture->setOwnsTexture(false);
            m_dirtyTexture = true;
        }

        if (m_dirtyTexture) {
            if (!m_gl) {
                // on a rendering thread, use the fbo directly...
                texture->setTextureId(m_fbo->texture());
            } else {
                // on GUI or custom thread, use display textures...
                m_displayTexture = m_displayTexture == 0 ? 1 : 0;
                texture->setTextureId(m_displayTextures[m_displayTexture]);
            }
            texture->setTextureSize(m_fbo->size());
            m_dirtyTexture = false;
        }

    }

    if (m_onCustomThread) {
        m_condition.wakeOne();
        m_mutex.unlock();
    }

    return texture;
}

QSize QQuickContext2DFBOTexture::adjustedTileSize(const QSize &ts)
{
    return npotAdjustedSize(ts);
}

QRectF QQuickContext2DFBOTexture::normalizedTextureSubRect() const
{
    return QRectF(0
                , 0
                , qreal(m_canvasWindow.width()) / m_fboSize.width()
                , qreal(m_canvasWindow.height()) / m_fboSize.height());
}

QQuickContext2DTile* QQuickContext2DFBOTexture::createTile() const
{
    return new QQuickContext2DFBOTile();
}

bool QQuickContext2DFBOTexture::doMultisampling() const
{
    static bool extensionsChecked = false;
    static bool multisamplingSupported = false;

    if (!extensionsChecked) {
        const QSet<QByteArray> extensions = QOpenGLContext::currentContext()->extensions();
        multisamplingSupported = extensions.contains(QByteArrayLiteral("GL_EXT_framebuffer_multisample"))
            && extensions.contains(QByteArrayLiteral("GL_EXT_framebuffer_blit"));
        extensionsChecked = true;
    }

    return multisamplingSupported  && m_antialiasing;
}

void QQuickContext2DFBOTexture::grabImage(const QRectF& rf)
{
    Q_ASSERT(rf.isValid());
    QQuickContext2D::mutex.lock();
    if (m_context) {
        if (!m_fbo) {
            m_context->setGrabbedImage(QImage());
        } else {
            QImage grabbed;
            GLAcquireContext ctx(m_gl, m_surface);
            grabbed = m_fbo->toImage().scaled(m_fboSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).mirrored().copy(rf.toRect());
            m_context->setGrabbedImage(grabbed);
        }
    }
    QQuickContext2D::mutex.unlock();
}

void QQuickContext2DFBOTexture::compositeTile(QQuickContext2DTile* tile)
{
    QQuickContext2DFBOTile* t = static_cast<QQuickContext2DFBOTile*>(tile);
    QRect target = t->rect().intersected(m_canvasWindow);
    if (target.isValid()) {
        QRect source = target;

        source.moveTo(source.topLeft() - t->rect().topLeft());
        target.moveTo(target.topLeft() - m_canvasWindow.topLeft());

        QOpenGLFramebufferObject::blitFramebuffer(m_fbo, target, t->fbo(), source);
    }
}

QQuickCanvasItem::RenderTarget QQuickContext2DFBOTexture::renderTarget() const
{
    return QQuickCanvasItem::FramebufferObject;
}

QPaintDevice* QQuickContext2DFBOTexture::beginPainting()
{
    QQuickContext2DTexture::beginPainting();

    const qreal devicePixelRatio = (m_item && m_item->window()) ?
        m_item->window()->effectiveDevicePixelRatio() : qApp->devicePixelRatio();

    if (m_canvasWindow.size().isEmpty()) {
        delete m_fbo;
        delete m_multisampledFbo;
        delete m_paint_device;
        m_fbo = 0;
        m_multisampledFbo = 0;
        m_paint_device = 0;
        return 0;
    } else if (!m_fbo || m_canvasWindowChanged) {
        delete m_fbo;
        delete m_multisampledFbo;
        delete m_paint_device;
        m_paint_device = 0;

        m_fboSize = npotAdjustedSize(m_canvasWindow.size() * devicePixelRatio);
        m_canvasWindowChanged = false;

        if (doMultisampling()) {
            {
                QOpenGLFramebufferObjectFormat format;
                format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
                format.setSamples(8);
                m_multisampledFbo = new QOpenGLFramebufferObject(m_fboSize, format);
            }
            {
                QOpenGLFramebufferObjectFormat format;
                format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
                m_fbo = new QOpenGLFramebufferObject(m_fboSize, format);
            }
        } else {
            QOpenGLFramebufferObjectFormat format;
            format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            QSize s = m_fboSize;
            if (m_antialiasing) { // do supersampling since multisampling is not available
                GLint max;
                QOpenGLContext::currentContext()->functions()->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
                if (s.width() * 2 <= max && s.height() * 2 <= max)
                    s = s * 2;
            }
            m_fbo = new QOpenGLFramebufferObject(s, format);
        }
    }

    if (doMultisampling())
        m_multisampledFbo->bind();
    else
        m_fbo->bind();

    if (!m_paint_device) {
        QOpenGLPaintDevice *gl_device = new QOpenGLPaintDevice(m_fbo->size());
        gl_device->setPaintFlipped(true);
        gl_device->setSize(m_fbo->size());
        gl_device->setDevicePixelRatio(devicePixelRatio);
        m_paint_device = gl_device;
    }

    return m_paint_device;
}

void QQuickContext2DFBOTexture::endPainting()
{
    QQuickContext2DTexture::endPainting();

    // There may not be an FBO due to zero width or height.
    if (!m_fbo)
        return;

    if (m_multisampledFbo)
        QOpenGLFramebufferObject::blitFramebuffer(m_fbo, m_multisampledFbo);

    if (m_gl) {
        /* When rendering happens on the render thread, the fbo's texture is
         * used directly for display. If we are on the GUI thread or a
         * dedicated Canvas render thread, we need to decouple the FBO from
         * the texture we are displaying in the SG rendering thread to avoid
         * stalls and read/write issues in the GL pipeline as the FBO's texture
         * could then potentially be used in different threads.
         *
         * We could have gotten away with only one display texture, but this
         * would have implied that beginPainting would have to wait for SG
         * to release that texture.
         */

        if (m_onCustomThread)
            m_mutex.lock();

        QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
        if (m_displayTextures[0] == 0) {
            m_displayTexture = 1;
            funcs->glGenTextures(2, m_displayTextures);
        }

        m_fbo->bind();
        GLuint target = m_displayTexture == 0 ? 1 : 0;
        funcs->glBindTexture(GL_TEXTURE_2D, m_displayTextures[target]);
        funcs->glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, m_fbo->width(), m_fbo->height(), 0);

        if (m_onCustomThread)
            m_mutex.unlock();
    }

    m_fbo->bindDefault();
}
#endif

QQuickContext2DImageTexture::QQuickContext2DImageTexture()
    : QQuickContext2DTexture()
{
}

QQuickContext2DImageTexture::~QQuickContext2DImageTexture()
{
}

QQuickCanvasItem::RenderTarget QQuickContext2DImageTexture::renderTarget() const
{
    return QQuickCanvasItem::Image;
}

QQuickContext2DTile* QQuickContext2DImageTexture::createTile() const
{
    return new QQuickContext2DImageTile();
}

void QQuickContext2DImageTexture::grabImage(const QRectF& rf)
{
    Q_ASSERT(rf.isValid());
    QQuickContext2D::mutex.lock();
    if (m_context) {
        QImage grabbed = m_displayImage.copy(rf.toRect());
        m_context->setGrabbedImage(grabbed);
    }
    QQuickContext2D::mutex.unlock();
}

QSGTexture *QQuickContext2DImageTexture::textureForNextFrame(QSGTexture *last, QQuickWindow *window)
{
    if (m_onCustomThread)
        m_mutex.lock();

    delete last;

    QSGTexture *texture = window->createTextureFromImage(m_displayImage, QQuickWindow::TextureCanUseAtlas);
    m_dirtyTexture = false;

    if (m_onCustomThread)
        m_mutex.unlock();

    return texture;
}

QPaintDevice* QQuickContext2DImageTexture::beginPainting()
{
    QQuickContext2DTexture::beginPainting();

    if (m_canvasWindow.size().isEmpty())
        return 0;

    const qreal devicePixelRatio = (m_item && m_item->window()) ?
        m_item->window()->effectiveDevicePixelRatio() : qApp->devicePixelRatio();

    if (m_canvasWindowChanged) {
        m_image = QImage(m_canvasWindow.size() * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
        m_image.setDevicePixelRatio(devicePixelRatio);
        m_image.fill(0x00000000);
        m_canvasWindowChanged = false;
    }

    return &m_image;
}

void QQuickContext2DImageTexture::endPainting()
{
    QQuickContext2DTexture::endPainting();
    if (m_onCustomThread)
        m_mutex.lock();
    m_displayImage = m_image;
    if (m_onCustomThread)
        m_mutex.unlock();
}

void QQuickContext2DImageTexture::compositeTile(QQuickContext2DTile* tile)
{
    Q_ASSERT(!tile->dirty());
    QQuickContext2DImageTile* t = static_cast<QQuickContext2DImageTile*>(tile);
    QRect target = t->rect().intersected(m_canvasWindow);
    if (target.isValid()) {
        QRect source = target;
        source.moveTo(source.topLeft() - t->rect().topLeft());
        target.moveTo(target.topLeft() - m_canvasWindow.topLeft());

        m_painter.begin(&m_image);
        m_painter.setCompositionMode(QPainter::CompositionMode_Source);
        m_painter.drawImage(target, t->image(), source);
        m_painter.end();
    }
}

QT_END_NAMESPACE
