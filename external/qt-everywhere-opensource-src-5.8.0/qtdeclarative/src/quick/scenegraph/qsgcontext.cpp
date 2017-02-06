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

#include <QtQuick/private/qsgcontext_p.h>
#include <QtQuick/private/qsgtexture_p.h>
#include <QtQuick/private/qquickpixmapcache_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>

#include <QGuiApplication>
#include <QScreen>
#include <QQuickWindow>

#include <private/qqmlglobal_p.h>

#include <QtQuick/private/qsgtexture_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtCore/private/qabstractanimation_p.h>

#include <private/qobject_p.h>
#include <qmutex.h>

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
Q_LOGGING_CATEGORY(QSG_LOG_INFO,                "qt.scenegraph.general")

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

bool qsg_useConsistentTiming()
{
    int use = -1;
    if (use < 0) {
        use = !qEnvironmentVariableIsEmpty("QSG_FIXED_ANIMATION_STEP") && qgetenv("QSG_FIXED_ANIMATION_STEP") != "no"
            ? 1 : 0;
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

    double m_time;
    double m_vsync;
    Mode m_mode;
    QElapsedTimer m_timer;
    QElapsedTimer m_wallTime;
    double m_lag;
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

/*!
    \class QSGContext

    \brief The QSGContext holds the scene graph entry points for one QML engine.

    The context is not ready for use until it has a QOpenGLContext. Once that happens,
    the scene graph population can start.

    \internal
 */

QSGContext::QSGContext(QObject *parent) :
    QObject(parent)
{
}

QSGContext::~QSGContext()
{
}

void QSGContext::renderContextInitialized(QSGRenderContext *)
{
}

void QSGContext::renderContextInvalidated(QSGRenderContext *)
{
}


/*!
    Convenience factory function for creating a colored rectangle with the given geometry.
 */
QSGInternalRectangleNode *QSGContext::createInternalRectangleNode(const QRectF &rect, const QColor &c)
{
    QSGInternalRectangleNode *node = createInternalRectangleNode();
    node->setRect(rect);
    node->setColor(c);
    node->update();
    return node;
}

/*!
    Creates a new shader effect helper instance. This function is called on the
    gui thread, unlike the others. This is necessary in order to provide
    adaptable, backend-specific shader effect functionality to the gui thread too.
 */
QSGGuiThreadShaderEffectManager *QSGContext::createGuiThreadShaderEffectManager()
{
    return nullptr;
}

/*!
    Creates a new shader effect node. The default of returning nullptr is
    valid as long as the backend does not claim SupportsShaderEffectNode or
    ignoring ShaderEffect elements is acceptable.
 */
QSGShaderEffectNode *QSGContext::createShaderEffectNode(QSGRenderContext *, QSGGuiThreadShaderEffectManager *)
{
    return nullptr;
}

/*!
    Creates a new animation driver.
 */
QAnimationDriver *QSGContext::createAnimationDriver(QObject *parent)
{
    return new QSGAnimationDriver(parent);
}

QSize QSGContext::minimumFBOSize() const
{
    return QSize(1, 1);
}

/*!
    Returns a pointer to the (presumably) global renderer interface.

    \note This function may be called on the gui thread in order to get access
    to QSGRendererInterface::graphicsApi() and other getters.

    \note it is expected that the simple queries (graphicsApi, shaderType,
    etc.) are available regardless of the render context validity (i.e.
    scenegraph status). This does not apply to engine-specific getters like
    getResource(). In the end this means that this function must always return
    a valid object in subclasses, even when renderContext->isValid() is false.
    The typical pattern is to implement the QSGRendererInterface in the
    QSGContext or QSGRenderContext subclass itself, whichever is more suitable.
 */
QSGRendererInterface *QSGContext::rendererInterface(QSGRenderContext *renderContext)
{
    Q_UNUSED(renderContext);
    qWarning("QSGRendererInterface not implemented");
    return nullptr;
}

QSGRenderContext::QSGRenderContext(QSGContext *context)
    : m_sg(context)
    , m_distanceFieldCacheManager(0)
{
}

QSGRenderContext::~QSGRenderContext()
{
}

void QSGRenderContext::initialize(void *context)
{
    Q_UNUSED(context);
}

void QSGRenderContext::invalidate()
{
}

void QSGRenderContext::endSync()
{
    qDeleteAll(m_texturesToDelete);
    m_texturesToDelete.clear();
}

/*!
    Factory function for scene graph backends of the distance-field glyph cache.
 */
QSGDistanceFieldGlyphCache *QSGRenderContext::distanceFieldGlyphCache(const QRawFont &)
{
    return nullptr;
}


void QSGRenderContext::registerFontengineForCleanup(QFontEngine *engine)
{
    engine->ref.ref();
    m_fontEnginesToClean << engine;
}

/*!
    Factory function for texture objects.

    If \a image is a valid image, the QSGTexture::setImage function
    will be called with \a image as argument.
 */

/*!
    Factory function for the scene graph renderers.

    The renderers are used for the toplevel renderer and once for every
    QQuickShaderEffectSource used in the QML scene.
 */

QSGTexture *QSGRenderContext::textureForFactory(QQuickTextureFactory *factory, QQuickWindow *window)
{
    if (!factory)
        return 0;

    m_mutex.lock();
    QSGTexture *texture = m_textures.value(factory);
    m_mutex.unlock();

    if (!texture) {
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

#include "qsgcontext.moc"

QT_END_NAMESPACE
