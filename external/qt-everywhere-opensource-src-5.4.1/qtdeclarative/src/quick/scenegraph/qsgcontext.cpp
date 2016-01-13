/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtQuick/private/qsgcontext_p.h>
#include <QtQuick/private/qsgbatchrenderer_p.h>
#include <QtQuick/private/qsgdistancefieldutil_p.h>
#include <QtQuick/private/qsgdefaultdistancefieldglyphcache_p.h>
#include <QtQuick/private/qsgdefaultrectanglenode_p.h>
#include <QtQuick/private/qsgdefaultimagenode_p.h>
#include <QtQuick/private/qsgdefaultpainternode_p.h>
#include <QtQuick/private/qsgdefaultglyphnode_p.h>
#include <QtQuick/private/qsgdistancefieldglyphnode_p.h>
#include <QtQuick/private/qsgdistancefieldglyphnode_p_p.h>
#include <QtQuick/private/qsgshareddistancefieldglyphcache_p.h>
#include <QtQuick/private/qsgatlastexture_p.h>
#include <QtQuick/private/qsgrenderloop_p.h>
#include <QtQuick/private/qsgdefaultlayer_p.h>

#include <QtQuick/private/qsgtexture_p.h>
#include <QtQuick/private/qquickpixmapcache_p.h>

#include <QGuiApplication>
#include <QScreen>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QtGui/qopenglframebufferobject.h>

#include <private/qqmlglobal_p.h>

#include <QtQuick/private/qsgtexture_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtCore/private/qabstractanimation_p.h>
#include <qpa/qplatformintegration.h>

#include <qpa/qplatformsharedgraphicscache.h>

#include <private/qobject_p.h>
#include <qmutex.h>

#include <private/qqmlprofilerservice_p.h>

DEFINE_BOOL_CONFIG_OPTION(qmlDisableDistanceField, QML_DISABLE_DISTANCEFIELD)

/*
    Comments about this class from Gunnar:

    The QSGContext class is right now two things.. The first is the
    adaptation layer and central storage ground for all the things
    in the scene graph, like textures and materials. This part really
    belongs inside the scene graph coreapi.

    The other part is the QML adaptation classes, like how to implement
    rectangle nodes. This is not part of the scene graph core API, but
    more part of the QML adaptation of scene graph.

    If we ever move the scene graph core API into its own thing, this class
    needs to be split in two. Right now its one because we're lazy when it comes
    to defining plugin interfaces..
*/

QT_BEGIN_NAMESPACE

// Used for very high-level info about the renderering and gl context
// Includes GL_VERSION, type of render loop, atlas size, etc.
Q_LOGGING_CATEGORY(QSG_LOG_INFO,                "qt.scenegraph.info")

// Used to debug the renderloop logic. Primarily useful for platform integrators
// and when investigating the render loop logic.
Q_LOGGING_CATEGORY(QSG_LOG_RENDERLOOP,          "qt.scenegraph.renderloop")


// GLSL shader compilation
Q_LOGGING_CATEGORY(QSG_LOG_TIME_COMPILATION,    "qt.scenegraph.time.compilation")

// polish, animations, sync, render and swap in the render loop
Q_LOGGING_CATEGORY(QSG_LOG_TIME_RENDERLOOP,     "qt.scenegraph.time.renderloop")

// Texture uploads and swizzling
Q_LOGGING_CATEGORY(QSG_LOG_TIME_TEXTURE,        "qt.scenegraph.time.texture")

// Glyph preparation (only for distance fields atm)
Q_LOGGING_CATEGORY(QSG_LOG_TIME_GLYPH,          "qt.scenegraph.time.glyph")

// Timing inside the renderer base class
Q_LOGGING_CATEGORY(QSG_LOG_TIME_RENDERER,       "qt.scenegraph.time.renderer")

class QSGContextPrivate : public QObjectPrivate
{
public:
    QSGContextPrivate()
        : antialiasingMethod(QSGContext::UndecidedAntialiasing)
        , distanceFieldDisabled(qmlDisableDistanceField())
        , distanceFieldAntialiasing(QSGGlyphNode::HighQualitySubPixelAntialiasing)
        , distanceFieldAntialiasingDecided(false)
    {
    }

    ~QSGContextPrivate()
    {
    }

    QMutex mutex;
    QSGContext::AntialiasingMethod antialiasingMethod;
    bool distanceFieldDisabled;
    QSGDistanceFieldGlyphNode::AntialiasingMode distanceFieldAntialiasing;
    bool distanceFieldAntialiasingDecided;
};

static bool qsg_useConsistentTiming()
{
    static int use = -1;
    if (use < 0) {
        QByteArray fixed = qgetenv("QSG_FIXED_ANIMATION_STEP");
        use = !(fixed.isEmpty() || fixed == "no");
        qCDebug(QSG_LOG_INFO, "Using %s", bool(use) ? "fixed animation steps" : "sg animation driver");
    }
    return bool(use);
}

class QSGAnimationDriver : public QAnimationDriver
{
    Q_OBJECT
public:
    enum Mode {
        VSyncMode,
        TimerMode
    };

    QSGAnimationDriver(QObject *parent)
        : QAnimationDriver(parent)
        , m_time(0)
        , m_vsync(0)
        , m_mode(VSyncMode)
        , m_lag(0)
        , m_bad(0)
        , m_good(0)
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen && !qsg_useConsistentTiming()) {
            m_vsync = 1000.0 / screen->refreshRate();
            if (m_vsync <= 0)
                m_mode = TimerMode;
        } else {
            m_mode = TimerMode;
            if (qsg_useConsistentTiming())
                QUnifiedTimer::instance(true)->setConsistentTiming(true);
        }
        if (m_mode == VSyncMode)
            qCDebug(QSG_LOG_INFO, "Animation Driver: using vsync: %.2f ms", m_vsync);
        else
            qCDebug(QSG_LOG_INFO, "Animation Driver: using walltime");
    }

    void start() Q_DECL_OVERRIDE
    {
        m_time = 0;
        m_timer.start();
        m_wallTime.restart();
        QAnimationDriver::start();
    }

    qint64 elapsed() const Q_DECL_OVERRIDE
    {
        return m_mode == VSyncMode
                ? qint64(m_time)
                : qint64(m_time) + m_wallTime.elapsed();
    }

    void advance() Q_DECL_OVERRIDE
    {
        qint64 delta = m_timer.restart();

        if (m_mode == VSyncMode) {
            // If a frame is skipped, either because rendering was slow or because
            // the QML was slow, we accept it and continue advancing with a single
            // vsync tick. The reason for this is that by the time we notice this
            // on the GUI thread, the temporal distortion has already gone to screen
            // and by catching up, we will introduce a second distortion which will
            // worse. We accept that the animation time falls behind wall time because
            // it comes out looking better.
            // Only when multiple bad frames are hit in a row, do we consider
            // switching. A few really bad frames and we switch right away. For frames
            // just above the vsync delta, we tolerate a bit more since a buffered
            // driver can have vsync deltas on the form: 4, 21, 21, 2, 23, 16, and
            // still manage to put the frames to screen at 16 ms intervals. In addition
            // to that, we tolerate a 25% margin of error on the value of m_vsync
            // reported from the system as this value is often not precise.

            m_time += m_vsync;

            if (delta > m_vsync * 1.25) {
                m_lag += (delta / m_vsync);
                m_bad++;
               // We tolerate one bad frame without resorting to timer based. This is
                // done to cope with a slow loader frame followed by smooth animation.
                // However, on the second frame with massive lag, we switch.
                if (m_lag > 10 && m_bad > 2) {
                    m_mode = TimerMode;
                    qCDebug(QSG_LOG_INFO, "animation driver switched to timer mode");
                    m_wallTime.restart();
                }
            } else {
                m_lag = 0;
                m_bad = 0;
            }

        } else {
            if (delta < 1.25 * m_vsync) {
                ++m_good;
            } else {
                m_good = 0;
            }

            // We've been solid for a while, switch back to vsync mode. Tolerance
            // for switching back is lower than switching to timer mode, as we
            // want to stay in vsync mode as much as possible.
            if (m_good > 10 && !qsg_useConsistentTiming()) {
                m_time = elapsed();
                m_mode = VSyncMode;
                m_bad = 0;
                m_lag = 0;
                qCDebug(QSG_LOG_INFO, "animation driver switched to vsync mode");
            }
        }

        advanceAnimation();
    }

    float m_time;
    float m_vsync;
    Mode m_mode;
    QElapsedTimer m_timer;
    QElapsedTimer m_wallTime;
    float m_lag;
    int m_bad;
    int m_good;
};

class QSGTextureCleanupEvent : public QEvent
{
public:
    QSGTextureCleanupEvent(QSGTexture *t) : QEvent(QEvent::User), texture(t) { }
    ~QSGTextureCleanupEvent() { delete texture; }
    QSGTexture *texture;
};

namespace QSGMultisampleAntialiasing {
    class ImageNode : public QSGDefaultImageNode {
    public:
        void setAntialiasing(bool) { }
    };


    class RectangleNode : public QSGDefaultRectangleNode {
    public:
        void setAntialiasing(bool) { }
    };
}


/*!
    \class QSGContext

    \brief The QSGContext holds the scene graph entry points for one QML engine.

    The context is not ready for use until it has a QOpenGLContext. Once that happens,
    the scene graph population can start.

    \internal
 */

QSGContext::QSGContext(QObject *parent) :
    QObject(*(new QSGContextPrivate), parent)
{
    Q_D(QSGContext);
    QByteArray mode = qgetenv("QSG_DISTANCEFIELD_ANTIALIASING");
    if (!mode.isEmpty())
        d->distanceFieldAntialiasingDecided = true;
    if (mode == "subpixel")
        d->distanceFieldAntialiasing = QSGGlyphNode::HighQualitySubPixelAntialiasing;
    else if (mode == "subpixel-lowq")
        d->distanceFieldAntialiasing = QSGGlyphNode::LowQualitySubPixelAntialiasing;
    else if (mode == "gray")
        d->distanceFieldAntialiasing = QSGGlyphNode::GrayAntialiasing;

    // Adds compatibility with Qt 5.3 and earlier's QSG_RENDER_TIMING
    if (qEnvironmentVariableIsSet("QSG_RENDER_TIMING")) {
        ((QLoggingCategory &) QSG_LOG_TIME_GLYPH()).setEnabled(QtDebugMsg, true);
        ((QLoggingCategory &) QSG_LOG_TIME_TEXTURE()).setEnabled(QtDebugMsg, true);
        ((QLoggingCategory &) QSG_LOG_TIME_RENDERER()).setEnabled(QtDebugMsg, true);
        ((QLoggingCategory &) QSG_LOG_TIME_RENDERLOOP()).setEnabled(QtDebugMsg, true);
        ((QLoggingCategory &) QSG_LOG_TIME_COMPILATION()).setEnabled(QtDebugMsg, true);
    }
}


QSGContext::~QSGContext()
{
}

QSGRenderContext *QSGContext::createRenderContext()
{
    return new QSGRenderContext(this);
}

void QSGContext::renderContextInitialized(QSGRenderContext *renderContext)
{
    Q_D(QSGContext);

    d->mutex.lock();
    if (d->antialiasingMethod == UndecidedAntialiasing) {
        QByteArray aaType = qgetenv("QSG_ANTIALIASING_METHOD");
        if (aaType == "msaa") {
            d->antialiasingMethod = MsaaAntialiasing;
        } else if (aaType == "vertex") {
            d->antialiasingMethod = VertexAntialiasing;
        } else {
            if (renderContext->openglContext()->format().samples() > 0)
                d->antialiasingMethod = MsaaAntialiasing;
            else
                d->antialiasingMethod = VertexAntialiasing;
        }
    }

    // With OpenGL ES, except for Angle on Windows, use GrayAntialiasing, unless
    // some value had been requested explicitly. This could not be decided
    // before without a context. Now the context is ready.
    if (!d->distanceFieldAntialiasingDecided) {
        d->distanceFieldAntialiasingDecided = true;
#ifndef Q_OS_WIN32
        if (renderContext->openglContext()->isOpenGLES())
            d->distanceFieldAntialiasing = QSGGlyphNode::GrayAntialiasing;
#endif
    }

    static bool dumped = false;
    if (!dumped && QSG_LOG_INFO().isDebugEnabled()) {
        dumped = true;
        QSurfaceFormat format = renderContext->openglContext()->format();
        QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
        qCDebug(QSG_LOG_INFO) << "R/G/B/A Buffers:   " << format.redBufferSize() << format.greenBufferSize() << format.blueBufferSize() << format.alphaBufferSize();
        qCDebug(QSG_LOG_INFO) << "Depth Buffer:      " << format.depthBufferSize();
        qCDebug(QSG_LOG_INFO) << "Stencil Buffer:    " << format.stencilBufferSize();
        qCDebug(QSG_LOG_INFO) << "Samples:           " << format.samples();
        qCDebug(QSG_LOG_INFO) << "GL_VENDOR:         " << (const char *) funcs->glGetString(GL_VENDOR);
        qCDebug(QSG_LOG_INFO) << "GL_RENDERER:       " << (const char *) funcs->glGetString(GL_RENDERER);
        qCDebug(QSG_LOG_INFO) << "GL_VERSION:        " << (const char *) funcs->glGetString(GL_VERSION);
        QSet<QByteArray> exts = renderContext->openglContext()->extensions();
        QByteArray all; foreach (const QByteArray &e, exts) all += ' ' + e;
        qCDebug(QSG_LOG_INFO) << "GL_EXTENSIONS:    " << all.constData();
        qCDebug(QSG_LOG_INFO) << "Max Texture Size: " << renderContext->maxTextureSize();
    }

    d->mutex.unlock();
}

void QSGContext::renderContextInvalidated(QSGRenderContext *)
{
}


/*!
    Convenience factory function for creating a colored rectangle with the given geometry.
 */
QSGRectangleNode *QSGContext::createRectangleNode(const QRectF &rect, const QColor &c)
{
    QSGRectangleNode *node = createRectangleNode();
    node->setRect(rect);
    node->setColor(c);
    node->update();
    return node;
}

/*!
    Factory function for scene graph backends of the Rectangle element.
 */
QSGRectangleNode *QSGContext::createRectangleNode()
{
    Q_D(QSGContext);
    return d->antialiasingMethod == MsaaAntialiasing
            ? new QSGMultisampleAntialiasing::RectangleNode
            : new QSGDefaultRectangleNode;
}

/*!
    Factory function for scene graph backends of the Image element.
 */
QSGImageNode *QSGContext::createImageNode()
{
    Q_D(QSGContext);
    return d->antialiasingMethod == MsaaAntialiasing
            ? new QSGMultisampleAntialiasing::ImageNode
            : new QSGDefaultImageNode;
}

/*!
    Factory function for scene graph backends of Painter elements
 */
QSGPainterNode *QSGContext::createPainterNode(QQuickPaintedItem *item)
{
    return new QSGDefaultPainterNode(item);
}

/*!
    Factory function for scene graph backends of the Text elements;
 */
QSGGlyphNode *QSGContext::createGlyphNode(QSGRenderContext *rc, bool preferNativeGlyphNode)
{
    Q_D(QSGContext);

    if (d->distanceFieldDisabled || preferNativeGlyphNode) {
        return new QSGDefaultGlyphNode;
    } else {
        QSGDistanceFieldGlyphNode *node = new QSGDistanceFieldGlyphNode(rc);
        node->setPreferredAntialiasingMode(d->distanceFieldAntialiasing);
        return node;
    }
}

/*!
 * Factory function for scene graph backends of the QStyle stylable elements. Returns a
 * null pointer if the backend doesn't provide its own node type.
 */
QSGNinePatchNode *QSGContext::createNinePatchNode()
{
    return 0;
}

/*!
    Factory function for scene graph backends of layers.
 */
QSGLayer *QSGContext::createLayer(QSGRenderContext *renderContext)
{
    return new QSGDefaultLayer(renderContext);
}

QSurfaceFormat QSGContext::defaultSurfaceFormat() const
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    static bool useDepth = qEnvironmentVariableIsEmpty("QSG_NO_DEPTH_BUFFER");
    static bool useStencil = qEnvironmentVariableIsEmpty("QSG_NO_STENCIL_BUFFER");
    format.setDepthBufferSize(useDepth ? 24 : 0);
    format.setStencilBufferSize(useStencil ? 8 : 0);
    if (QQuickWindow::hasDefaultAlphaBuffer())
        format.setAlphaBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    return format;
}

/*!
    Returns the minimum supported framebuffer object size.
 */

QSize QSGContext::minimumFBOSize() const
{
#ifdef Q_OS_MAC
    if (QSysInfo::MacintoshVersion < QSysInfo::MV_10_8)
        return QSize(33, 33);
#endif
    return QSize(1, 1);
}



/*!
    Sets whether or not the scene graph should use the distance field technique to render text
  */
void QSGContext::setDistanceFieldEnabled(bool enabled)
{
    d_func()->distanceFieldDisabled = !enabled;
}


/*!
    Returns true if the scene graph uses the distance field technique to render text
 */
bool QSGContext::isDistanceFieldEnabled() const
{
    return !d_func()->distanceFieldDisabled;
}



/*!
    Creates a new animation driver.
 */

QAnimationDriver *QSGContext::createAnimationDriver(QObject *parent)
{
    return new QSGAnimationDriver(parent);
}

QSGRenderContext::QSGRenderContext(QSGContext *context)
    : m_gl(0)
    , m_sg(context)
    , m_atlasManager(0)
    , m_depthStencilManager(0)
    , m_distanceFieldCacheManager(0)
    , m_maxTextureSize(0)
    , m_brokenIBOs(false)
    , m_serializedRender(false)
    , m_attachToGLContext(true)
{
}

QSGRenderContext::~QSGRenderContext()
{
    invalidate();
}

void QSGRenderContext::endSync()
{
    qDeleteAll(m_texturesToDelete);
    m_texturesToDelete.clear();
}

static QBasicMutex qsg_framerender_mutex;

void QSGRenderContext::renderNextFrame(QSGRenderer *renderer, GLuint fboId)
{
    if (m_serializedRender)
        qsg_framerender_mutex.lock();

    renderer->renderScene(fboId);

    if (m_serializedRender)
        qsg_framerender_mutex.unlock();

}

/*!
    Factory function for scene graph backends of the distance-field glyph cache.
 */
QSGDistanceFieldGlyphCache *QSGRenderContext::distanceFieldGlyphCache(const QRawFont &font)
{
    if (!m_distanceFieldCacheManager)
        m_distanceFieldCacheManager = new QSGDistanceFieldGlyphCacheManager;

    QSGDistanceFieldGlyphCache *cache = m_distanceFieldCacheManager->cache(font);
    if (!cache) {
        QPlatformIntegration *platformIntegration = QGuiApplicationPrivate::platformIntegration();
        if (platformIntegration != 0
            && platformIntegration->hasCapability(QPlatformIntegration::SharedGraphicsCache)) {
            QFontEngine *fe = QRawFontPrivate::get(font)->fontEngine;
            if (!fe->faceId().filename.isEmpty()) {
                QByteArray keyName = fe->faceId().filename;
                if (font.style() != QFont::StyleNormal)
                    keyName += QByteArray(" I");
                if (font.weight() != QFont::Normal)
                    keyName += ' ' + QByteArray::number(font.weight());
                keyName += QByteArray(" DF");
                QPlatformSharedGraphicsCache *sharedGraphicsCache =
                        platformIntegration->createPlatformSharedGraphicsCache(keyName);

                if (sharedGraphicsCache != 0) {
                    sharedGraphicsCache->ensureCacheInitialized(keyName,
                                                                QPlatformSharedGraphicsCache::OpenGLTexture,
                                                                QPlatformSharedGraphicsCache::Alpha8);

                    cache = new QSGSharedDistanceFieldGlyphCache(keyName,
                                                                 sharedGraphicsCache,
                                                                 m_distanceFieldCacheManager,
                                                                 openglContext(),
                                                                 font);
                }
            }
        }
        if (!cache)
            cache = new QSGDefaultDistanceFieldGlyphCache(m_distanceFieldCacheManager, openglContext(), font);
        m_distanceFieldCacheManager->insertCache(font, cache);
    }

    return cache;
}

void QSGRenderContext::setAttachToGLContext(bool attach)
{
    Q_ASSERT(!isValid());
    m_attachToGLContext = attach;
}

#define QSG_RENDERCONTEXT_PROPERTY "_q_sgrendercontext"

QSGRenderContext *QSGRenderContext::from(QOpenGLContext *context)
{
    return qobject_cast<QSGRenderContext *>(context->property(QSG_RENDERCONTEXT_PROPERTY).value<QObject *>());
}

void QSGRenderContext::registerFontengineForCleanup(QFontEngine *engine)
{
    engine->ref.ref();
    m_fontEnginesToClean << engine;
}

/*!
    Initializes the scene graph render context with the GL context \a context. This also
    emits the ready() signal so that the QML graph can start building scene graph nodes.
 */
void QSGRenderContext::initialize(QOpenGLContext *context)
{
    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    funcs->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);

    // Sanity check the surface format, in case it was overridden by the application
    QSurfaceFormat requested = m_sg->defaultSurfaceFormat();
    QSurfaceFormat actual = context->format();
    if (requested.depthBufferSize() > 0 && actual.depthBufferSize() <= 0)
        qWarning("QSGContext::initialize: depth buffer support missing, expect rendering errors");
    if (requested.stencilBufferSize() > 0 && actual.stencilBufferSize() <= 0)
        qWarning("QSGContext::initialize: stencil buffer support missing, expect rendering errors");

    if (!m_atlasManager)
        m_atlasManager = new QSGAtlasTexture::Manager();

    Q_ASSERT_X(!m_gl, "QSGRenderContext::initialize", "already initialized!");
    m_gl = context;
    if (m_attachToGLContext) {
        Q_ASSERT(!context->property(QSG_RENDERCONTEXT_PROPERTY).isValid());
        context->setProperty(QSG_RENDERCONTEXT_PROPERTY, QVariant::fromValue(this));
    }
    m_sg->renderContextInitialized(this);

#ifdef Q_OS_LINUX
    const char *vendor = (const char *) funcs->glGetString(GL_VENDOR);
    if (strstr(vendor, "nouveau"))
        m_brokenIBOs = true;
    const char *renderer = (const char *) funcs->glGetString(GL_RENDERER);
    if (strstr(renderer, "llvmpipe"))
        m_serializedRender = true;
    if (strstr(vendor, "Hisilicon Technologies") && strstr(renderer, "Immersion.16"))
        m_brokenIBOs = true;
#endif

    emit initialized();
}

void QSGRenderContext::invalidate()
{
    if (!m_gl)
        return;

    qDeleteAll(m_texturesToDelete);
    m_texturesToDelete.clear();

    qDeleteAll(m_textures.values());
    m_textures.clear();

    /* The cleanup of the atlas textures is a bit intriguing.
       As part of the cleanup in the threaded render loop, we
       do:
       1. call this function
       2. call QCoreApp::sendPostedEvents() to immediately process
          any pending deferred deletes.
       3. delete the GL context.

       As textures need the atlas manager while cleaning up, the
       manager needs to be cleaned up after the textures, so
       we post a deleteLater here at the very bottom so it gets
       deferred deleted last.

       Another alternative would be to use a QPointer in
       QSGAtlasTexture::Texture, but this seemed simpler.
     */
    m_atlasManager->invalidate();
    m_atlasManager->deleteLater();
    m_atlasManager = 0;

    // The following piece of code will read/write to the font engine's caches,
    // potentially from different threads. However, this is safe because this
    // code is only called from QQuickWindow's shutdown which is called
    // only when the GUI is blocked, and multiple threads will call it in
    // sequence. (see qsgdefaultglyphnode_p.cpp's init())
    for (QSet<QFontEngine *>::const_iterator it = m_fontEnginesToClean.constBegin(),
         end = m_fontEnginesToClean.constEnd(); it != end; ++it) {
        (*it)->clearGlyphCache(m_gl);
        if (!(*it)->ref.deref())
            delete *it;
    }
    m_fontEnginesToClean.clear();

    delete m_depthStencilManager;
    m_depthStencilManager = 0;

    delete m_distanceFieldCacheManager;
    m_distanceFieldCacheManager = 0;

    if (m_gl->property(QSG_RENDERCONTEXT_PROPERTY) == QVariant::fromValue(this))
        m_gl->setProperty(QSG_RENDERCONTEXT_PROPERTY, QVariant());
    m_gl = 0;

    m_sg->renderContextInvalidated(this);
    emit invalidated();
}

/*!
    Returns a shared pointer to a depth stencil buffer that can be used with \a fbo.
  */
QSharedPointer<QSGDepthStencilBuffer> QSGRenderContext::depthStencilBufferForFbo(QOpenGLFramebufferObject *fbo)
{
    if (!m_gl)
        return QSharedPointer<QSGDepthStencilBuffer>();
    QSGDepthStencilBufferManager *manager = depthStencilBufferManager();
    QSGDepthStencilBuffer::Format format;
    format.size = fbo->size();
    format.samples = fbo->format().samples();
    format.attachments = QSGDepthStencilBuffer::DepthAttachment | QSGDepthStencilBuffer::StencilAttachment;
    QSharedPointer<QSGDepthStencilBuffer> buffer = manager->bufferForFormat(format);
    if (buffer.isNull()) {
        buffer = QSharedPointer<QSGDepthStencilBuffer>(new QSGDefaultDepthStencilBuffer(m_gl, format));
        manager->insertBuffer(buffer);
    }
    return buffer;
}

/*!
    Returns a pointer to the context's depth/stencil buffer manager. This is useful for custom
    implementations of \l depthStencilBufferForFbo().
  */
QSGDepthStencilBufferManager *QSGRenderContext::depthStencilBufferManager()
{
    if (!m_gl)
        return 0;
    if (!m_depthStencilManager)
        m_depthStencilManager = new QSGDepthStencilBufferManager(m_gl);
    return m_depthStencilManager;
}


/*!
    Factory function for texture objects.

    If \a image is a valid image, the QSGTexture::setImage function
    will be called with \a image as argument.
 */

QSGTexture *QSGRenderContext::createTexture(const QImage &image) const
{
    if (!openglContext())
        return 0;
    QSGTexture *t = m_atlasManager->create(image);
    if (t)
        return t;
    return createTextureNoAtlas(image);
}

QSGTexture *QSGRenderContext::createTextureNoAtlas(const QImage &image) const
{
    QSGPlainTexture *t = new QSGPlainTexture();
    if (!image.isNull())
        t->setImage(image);
    return t;
}

/*!
    Factory function for the scene graph renderers.

    The renderers are used for the toplevel renderer and once for every
    QQuickShaderEffectSource used in the QML scene.
 */
QSGRenderer *QSGRenderContext::createRenderer()
{
    return new QSGBatchRenderer::Renderer(this);
}

QSGTexture *QSGRenderContext::textureForFactory(QQuickTextureFactory *factory, QQuickWindow *window)
{
    if (!factory)
        return 0;

    m_mutex.lock();
    QSGTexture *texture = m_textures.value(factory);
    m_mutex.unlock();

    if (!texture) {
        if (QQuickDefaultTextureFactory *dtf = qobject_cast<QQuickDefaultTextureFactory *>(factory))
            texture = createTexture(dtf->image());
        else
            texture = factory->createTexture(window);

        m_mutex.lock();
        m_textures.insert(factory, texture);
        m_mutex.unlock();

        connect(factory, SIGNAL(destroyed(QObject*)), this, SLOT(textureFactoryDestroyed(QObject*)), Qt::DirectConnection);
    }
    return texture;
}

void QSGRenderContext::textureFactoryDestroyed(QObject *o)
{
    m_mutex.lock();
    m_texturesToDelete << m_textures.take(static_cast<QQuickTextureFactory *>(o));
    m_mutex.unlock();
}

/*!
    Compile \a shader, optionally using \a vertexCode and \a fragmentCode as
    replacement for the source code supplied by \a shader.

    If \a vertexCode or \a fragmentCode is supplied, the caller is responsible
    for setting up attribute bindings.

    \a material is supplied in case the implementation needs to take the
    material flags into account.
 */

void QSGRenderContext::compile(QSGMaterialShader *shader, QSGMaterial *material, const char *vertexCode, const char *fragmentCode)
{
    Q_UNUSED(material);
    if (vertexCode || fragmentCode) {
        Q_ASSERT_X((material->flags() & QSGMaterial::CustomCompileStep) == 0,
                   "QSGRenderContext::compile()",
                   "materials with custom compile step cannot have custom vertex/fragment code");
        QOpenGLShaderProgram *p = shader->program();
        p->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexCode ? vertexCode : shader->vertexShader());
        p->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentCode ? fragmentCode : shader->fragmentShader());
        p->link();
        if (!p->isLinked())
            qWarning() << "shader compilation failed:" << endl << p->log();
    } else {
        shader->compile();
    }
}

void QSGRenderContext::initialize(QSGMaterialShader *shader)
{
    shader->program()->bind();
    shader->initialize();
}

#include "qsgcontext.moc"

QT_END_NAMESPACE
