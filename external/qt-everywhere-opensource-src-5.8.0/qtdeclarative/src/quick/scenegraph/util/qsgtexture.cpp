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

#include "qsgtexture_p.h"
#include <QtQuick/private/qsgcontext_p.h>
#include <qthread.h>
#include <qmath.h>
#include <private/qquickprofiler_p.h>
#include <private/qqmlglobal_p.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpa/qplatformnativeinterface.h>
#ifndef QT_NO_OPENGL
# include <qopenglfunctions.h>
# include <QtGui/qopenglcontext.h>
# include <QtGui/qopenglfunctions.h>
# include <private/qsgdefaultrendercontext_p.h>
#endif
#include <private/qsgmaterialshader_p.h>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && !defined(__UCLIBC__)
#define CAN_BACKTRACE_EXECINFO
#endif

#if defined(Q_OS_MAC)
#define CAN_BACKTRACE_EXECINFO
#endif

#if defined(QT_NO_DEBUG)
#undef CAN_BACKTRACE_EXECINFO
#endif

#if defined(CAN_BACKTRACE_EXECINFO)
#include <execinfo.h>
#include <QHash>
#endif

#ifndef QT_NO_OPENGL
static QElapsedTimer qsg_renderer_timer;
#endif

#ifndef QT_NO_DEBUG
static const bool qsg_leak_check = !qEnvironmentVariableIsEmpty("QML_LEAK_CHECK");
#endif


#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif


QT_BEGIN_NAMESPACE

#ifndef QT_NO_OPENGL
inline static bool isPowerOfTwo(int x)
{
    // Assumption: x >= 1
    return x == (x & -x);
}
#endif

QSGTexturePrivate::QSGTexturePrivate()
    : wrapChanged(false)
    , filteringChanged(false)
    , horizontalWrap(QSGTexture::ClampToEdge)
    , verticalWrap(QSGTexture::ClampToEdge)
    , mipmapMode(QSGTexture::None)
    , filterMode(QSGTexture::Nearest)
{
}

#ifndef QT_NO_DEBUG

static int qt_debug_texture_count = 0;

#if (defined(Q_OS_LINUX) || defined (Q_OS_MAC)) && !defined(Q_OS_ANDROID)
DEFINE_BOOL_CONFIG_OPTION(qmlDebugLeakBacktrace, QML_DEBUG_LEAK_BACKTRACE)

#define BACKTRACE_SIZE 20
class SGTextureTraceItem
{
public:
    void *backTrace[BACKTRACE_SIZE];
    size_t backTraceSize;
};

static QHash<QSGTexture*, SGTextureTraceItem*> qt_debug_allocated_textures;
#endif

inline static void qt_debug_print_texture_count()
{
    qDebug("Number of leaked textures: %i", qt_debug_texture_count);
    qt_debug_texture_count = -1;

#if defined(CAN_BACKTRACE_EXECINFO)
    if (qmlDebugLeakBacktrace()) {
        while (!qt_debug_allocated_textures.isEmpty()) {
            QHash<QSGTexture*, SGTextureTraceItem*>::Iterator it = qt_debug_allocated_textures.begin();
            QSGTexture* texture = it.key();
            SGTextureTraceItem* item = it.value();

            qt_debug_allocated_textures.erase(it);

            qDebug() << "------";
            qDebug() << "Leaked" << texture << "backtrace:";

            char** symbols = backtrace_symbols(item->backTrace, item->backTraceSize);

            if (symbols) {
                for (int i=0; i<(int) item->backTraceSize; i++)
                    qDebug("Backtrace <%02d>: %s", i, symbols[i]);
                free(symbols);
            }

            qDebug() << "------";

            delete item;
        }
    }
#endif
}

inline static void qt_debug_add_texture(QSGTexture* texture)
{
#if defined(CAN_BACKTRACE_EXECINFO)
    if (qmlDebugLeakBacktrace()) {
        SGTextureTraceItem* item = new SGTextureTraceItem;
        item->backTraceSize = backtrace(item->backTrace, BACKTRACE_SIZE);
        qt_debug_allocated_textures.insert(texture, item);
    }
#else
    Q_UNUSED(texture);
#endif // Q_OS_LINUX

    ++qt_debug_texture_count;

    static bool atexit_registered = false;
    if (!atexit_registered) {
        atexit(qt_debug_print_texture_count);
        atexit_registered = true;
    }
}

static void qt_debug_remove_texture(QSGTexture* texture)
{
#if defined(CAN_BACKTRACE_EXECINFO)
    if (qmlDebugLeakBacktrace()) {
        SGTextureTraceItem* item = qt_debug_allocated_textures.value(texture, 0);
        if (item) {
            qt_debug_allocated_textures.remove(texture);
            delete item;
        }
    }
#else
    Q_UNUSED(texture)
#endif

    --qt_debug_texture_count;

    if (qt_debug_texture_count < 0)
        qDebug("Texture destroyed after qt_debug_print_texture_count() was called.");
}

#endif // QT_NO_DEBUG

/*!
    \class QSGTexture

    \inmodule QtQuick

    \brief The QSGTexture class is a baseclass for textures used in
    the scene graph.


    Users can freely implement their own texture classes to support
    arbitrary input textures, such as YUV video frames or 8 bit alpha
    masks. The scene graph backend provides a default implementation
    of normal color textures. As the implementation of these may be
    hardware specific, they are constructed via the factory
    function QQuickWindow::createTextureFromImage().

    The texture is a wrapper around an OpenGL texture, which texture
    id is given by textureId() and which size in pixels is given by
    textureSize(). hasAlphaChannel() reports if the texture contains
    opacity values and hasMipmaps() reports if the texture contains
    mipmap levels.

    To use a texture, call the bind() function. The texture parameters
    specifying how the texture is bound, can be specified with
    setMipmapFiltering(), setFiltering(), setHorizontalWrapMode() and
    setVerticalWrapMode(). The texture will internally try to store
    these values to minimize the OpenGL state changes when the texture
    is bound.

    \section1 Texture Atlasses

    Some scene graph backends use texture atlasses, grouping multiple
    small textures into one large texture. If this is the case, the
    function isAtlasTexture() will return true. Atlasses are used to
    aid the rendering algorithm to do better sorting which increases
    performance. The location of the texture inside the atlas is
    given with the normalizedTextureSubRect() function.

    If the texture is used in such a way that atlas is not preferable,
    the function removedFromAtlas() can be used to extract a
    non-atlassed copy.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.

    \sa {Scene Graph - Rendering FBOs}, {Scene Graph - Rendering FBOs in a thread}
 */

/*!
    \enum QSGTexture::WrapMode

    Specifies how the texture should treat texture coordinates.

    \value Repeat Only the factional part of the texture coordiante is
    used, causing values above 1 and below 0 to repeat.

    \value ClampToEdge Values above 1 are clamped to 1 and values
    below 0 are clamped to 0.
 */

/*!
    \enum QSGTexture::Filtering

    Specifies how sampling of texels should filter when texture
    coordinates are not pixel aligned.

    \value None No filtering should occur. This value is only used
    together with setMipmapFiltering().

    \value Nearest Sampling returns the nearest texel.

    \value Linear Sampling returns a linear interpolation of the
    neighboring texels.
*/

/*!
    \fn QSGTexture::QSGTexture(QSGTexturePrivate &dd)
    \internal
 */

#ifndef QT_NO_DEBUG
Q_GLOBAL_STATIC(QSet<QSGTexture *>, qsg_valid_texture_set)
Q_GLOBAL_STATIC(QMutex, qsg_valid_texture_mutex)

bool qsg_safeguard_texture(QSGTexture *texture)
{
#ifndef QT_NO_OPENGL
    QMutexLocker locker(qsg_valid_texture_mutex());
    if (!qsg_valid_texture_set()->contains(texture)) {
        qWarning() << "Invalid texture accessed:" << (void *) texture;
        qsg_set_material_failure();
        QOpenGLContext::currentContext()->functions()->glBindTexture(GL_TEXTURE_2D, 0);
        return false;
    }
#else
    Q_UNUSED(texture)
#endif
    return true;
}
#endif

/*!
    Constructs the QSGTexture base class.
 */
QSGTexture::QSGTexture()
    : QObject(*(new QSGTexturePrivate))
{
#ifndef QT_NO_DEBUG
    if (qsg_leak_check)
        qt_debug_add_texture(this);

    QMutexLocker locker(qsg_valid_texture_mutex());
    qsg_valid_texture_set()->insert(this);
#endif
}

/*!
    Destroys the QSGTexture.
 */
QSGTexture::~QSGTexture()
{
#ifndef QT_NO_DEBUG
    if (qsg_leak_check)
        qt_debug_remove_texture(this);

    QMutexLocker locker(qsg_valid_texture_mutex());
    qsg_valid_texture_set()->remove(this);
#endif
}


/*!
    \fn void QSGTexture::bind()

    Call this function to bind this texture to the current texture
    target.

    Binding a texture may also include uploading the texture data from
    a previously set QImage.

    \warning This function can only be called from the rendering thread.
 */

/*!
    \fn QRectF QSGTexture::convertToNormalizedSourceRect(const QRectF &rect) const

    Returns \a rect converted to normalized coordinates.

    \sa normalizedTextureSubRect()
 */

/*!
    This function returns a copy of the current texture which is removed
    from its atlas.

    The current texture remains unchanged, so texture coordinates do not
    need to be updated.

    Removing a texture from an atlas is primarily useful when passing
    it to a shader that operates on the texture coordinates 0-1 instead
    of the texture subrect inside the atlas.

    If the texture is not part of a texture atlas, this function returns 0.

    Implementations of this function are recommended to return the same instance
    for multiple calls to limit memory usage.

    \warning This function can only be called from the rendering thread.
 */

QSGTexture *QSGTexture::removedFromAtlas() const
{
    Q_ASSERT_X(!isAtlasTexture(), "QSGTexture::removedFromAtlas()", "Called on a non-atlas texture");
    return 0;
}

/*!
    Returns weither this texture is part of an atlas or not.

    The default implementation returns false.
 */
bool QSGTexture::isAtlasTexture() const
{
    return false;
}

/*!
    \fn int QSGTexture::textureId() const

    Returns the OpenGL texture id for this texture.

    The default value is 0, indicating that it is an invalid texture id.

    The function should at all times return the correct texture id.

    \warning This function can only be called from the rendering thread.
 */

/*!
    \fn QSize QSGTexture::textureSize() const

    Returns the size of the texture.
 */

/*!
    Returns the rectangle inside textureSize() that this texture
    represents in normalized coordinates.

    The default implementation returns a rect at position (0, 0) with
    width and height of 1.
 */
QRectF QSGTexture::normalizedTextureSubRect() const
{
    return QRectF(0, 0, 1, 1);
}

/*!
    \fn bool QSGTexture::hasAlphaChannel() const

    Returns true if the texture data contains an alpha channel.
 */

/*!
    \fn bool QSGTexture::hasMipmaps() const

    Returns true if the texture data contains mipmap levels.
 */


/*!
    Sets the mipmap sampling mode to be used for the upcoming bind() call to \a filter.

    Setting the mipmap filtering has no effect it the texture does not have mipmaps.

    \sa hasMipmaps()
 */
void QSGTexture::setMipmapFiltering(Filtering filter)
{
    Q_D(QSGTexture);
    if (d->mipmapMode != (uint) filter) {
        d->mipmapMode = filter;
        d->filteringChanged = true;
    }
}

/*!
    Returns whether mipmapping should be used when sampling from this texture.
 */
QSGTexture::Filtering QSGTexture::mipmapFiltering() const
{
    return (QSGTexture::Filtering) d_func()->mipmapMode;
}


/*!
    Sets the sampling mode to be used for the upcoming bind() call to \a filter.
 */
void QSGTexture::setFiltering(QSGTexture::Filtering filter)
{
    Q_D(QSGTexture);
    if (d->filterMode != (uint) filter) {
        d->filterMode = filter;
        d->filteringChanged = true;
    }
}

/*!
    Returns the sampling mode to be used for this texture.
 */
QSGTexture::Filtering QSGTexture::filtering() const
{
    return (QSGTexture::Filtering) d_func()->filterMode;
}



/*!
    Sets the horizontal wrap mode to be used for the upcoming bind() call to \a hwrap
 */

void QSGTexture::setHorizontalWrapMode(WrapMode hwrap)
{
    Q_D(QSGTexture);
    if ((uint) hwrap != d->horizontalWrap) {
        d->horizontalWrap = hwrap;
        d->wrapChanged = true;
    }
}

/*!
    Returns the horizontal wrap mode to be used for this texture.
 */
QSGTexture::WrapMode QSGTexture::horizontalWrapMode() const
{
    return (QSGTexture::WrapMode) d_func()->horizontalWrap;
}



/*!
    Sets the vertical wrap mode to be used for the upcoming bind() call to \a vwrap
 */
void QSGTexture::setVerticalWrapMode(WrapMode vwrap)
{
    Q_D(QSGTexture);
    if ((uint) vwrap != d->verticalWrap) {
        d->verticalWrap = vwrap;
        d->wrapChanged = true;
    }
}

/*!
    Returns the vertical wrap mode to be used for this texture.
 */
QSGTexture::WrapMode QSGTexture::verticalWrapMode() const
{
    return (QSGTexture::WrapMode) d_func()->verticalWrap;
}


/*!
    Update the texture state to match the filtering, mipmap and wrap options
    currently set.

    If \a force is true, all properties will be updated regardless of weither
    they have changed or not.
 */
void QSGTexture::updateBindOptions(bool force)
{
#ifndef QT_NO_OPENGL
    Q_D(QSGTexture);
    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    force |= isAtlasTexture();

    if (force || d->filteringChanged) {
        bool linear = d->filterMode == Linear;
        GLint minFilter = linear ? GL_LINEAR : GL_NEAREST;
        GLint magFilter = linear ? GL_LINEAR : GL_NEAREST;

        if (hasMipmaps()) {
            if (d->mipmapMode == Nearest)
                minFilter = linear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
            else if (d->mipmapMode == Linear)
                minFilter = linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
        }
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
        d->filteringChanged = false;
    }

    if (force || d->wrapChanged) {
#ifndef QT_NO_DEBUG
        if (d->horizontalWrap == Repeat || d->verticalWrap == Repeat) {
            bool npotSupported = QOpenGLFunctions(QOpenGLContext::currentContext()).hasOpenGLFeature(QOpenGLFunctions::NPOTTextures);
            QSize size = textureSize();
            bool isNpot = !isPowerOfTwo(size.width()) || !isPowerOfTwo(size.height());
            if (!npotSupported && isNpot)
                qWarning("Scene Graph: This system does not support the REPEAT wrap mode for non-power-of-two textures.");
        }
#endif
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, d->horizontalWrap == Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, d->verticalWrap == Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        d->wrapChanged = false;
    }
#else
    Q_UNUSED(force)
#endif
}

QSGPlainTexture::QSGPlainTexture()
    : QSGTexture()
    , m_texture_id(0)
    , m_has_alpha(false)
    , m_dirty_texture(false)
    , m_dirty_bind_options(false)
    , m_owns_texture(true)
    , m_mipmaps_generated(false)
    , m_retain_image(false)
{
}


QSGPlainTexture::~QSGPlainTexture()
{
#ifndef QT_NO_OPENGL
    if (m_texture_id && m_owns_texture && QOpenGLContext::currentContext())
        QOpenGLContext::currentContext()->functions()->glDeleteTextures(1, &m_texture_id);
#endif
}

void qsg_swizzleBGRAToRGBA(QImage *image)
{
    const int width = image->width();
    const int height = image->height();
    for (int i = 0; i < height; ++i) {
        uint *p = (uint *) image->scanLine(i);
        for (int x = 0; x < width; ++x)
            p[x] = ((p[x] << 16) & 0xff0000) | ((p[x] >> 16) & 0xff) | (p[x] & 0xff00ff00);
    }
}

void QSGPlainTexture::setImage(const QImage &image)
{
    m_image = image;
    m_texture_size = image.size();
    m_has_alpha = image.hasAlphaChannel();
    m_dirty_texture = true;
    m_dirty_bind_options = true;
    m_mipmaps_generated = false;
 }

int QSGPlainTexture::textureId() const
{
    if (m_dirty_texture) {
        if (m_image.isNull()) {
            // The actual texture and id will be updated/deleted in a later bind()
            // or ~QSGPlainTexture so just keep it minimal here.
            return 0;
        } else if (m_texture_id == 0){
#ifndef QT_NO_OPENGL
            // Generate a texture id for use later and return it.
            QOpenGLContext::currentContext()->functions()->glGenTextures(1, &const_cast<QSGPlainTexture *>(this)->m_texture_id);
#endif
            return m_texture_id;
        }
    }
    return m_texture_id;
}

void QSGPlainTexture::setTextureId(int id)
{
#ifndef QT_NO_OPENGL
    if (m_texture_id && m_owns_texture)
        QOpenGLContext::currentContext()->functions()->glDeleteTextures(1, &m_texture_id);
#endif

    m_texture_id = id;
    m_dirty_texture = false;
    m_dirty_bind_options = true;
    m_image = QImage();
    m_mipmaps_generated = false;
}

void QSGPlainTexture::bind()
{
#ifndef QT_NO_OPENGL
    QOpenGLContext *context = QOpenGLContext::currentContext();
    QOpenGLFunctions *funcs = context->functions();
    if (!m_dirty_texture) {
        funcs->glBindTexture(GL_TEXTURE_2D, m_texture_id);
        if (mipmapFiltering() != QSGTexture::None && !m_mipmaps_generated) {
            funcs->glGenerateMipmap(GL_TEXTURE_2D);
            m_mipmaps_generated = true;
        }
        updateBindOptions(m_dirty_bind_options);
        m_dirty_bind_options = false;
        return;
    }

    m_dirty_texture = false;

    bool profileFrames = QSG_LOG_TIME_TEXTURE().isDebugEnabled();
    if (profileFrames)
        qsg_renderer_timer.start();
    Q_QUICK_SG_PROFILE_START_SYNCHRONIZED(QQuickProfiler::SceneGraphTexturePrepare,
                                          QQuickProfiler::SceneGraphTextureDeletion);


    if (m_image.isNull()) {
        if (m_texture_id && m_owns_texture) {
            funcs->glDeleteTextures(1, &m_texture_id);
            qCDebug(QSG_LOG_TIME_TEXTURE, "plain texture deleted in %dms - %dx%d",
                    (int) qsg_renderer_timer.elapsed(),
                    m_texture_size.width(),
                    m_texture_size.height());
            Q_QUICK_SG_PROFILE_END(QQuickProfiler::SceneGraphTextureDeletion,
                                   QQuickProfiler::SceneGraphTextureDeletionDelete);
        }
        m_texture_id = 0;
        m_texture_size = QSize();
        m_has_alpha = false;

        return;
    }

    if (m_texture_id == 0)
        funcs->glGenTextures(1, &m_texture_id);
    funcs->glBindTexture(GL_TEXTURE_2D, m_texture_id);

    qint64 bindTime = 0;
    if (profileFrames)
        bindTime = qsg_renderer_timer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphTexturePrepare,
                              QQuickProfiler::SceneGraphTexturePrepareBind);

    // ### TODO: check for out-of-memory situations...

    QImage tmp = (m_image.format() == QImage::Format_RGB32 || m_image.format() == QImage::Format_ARGB32_Premultiplied)
                 ? m_image
                 : m_image.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // Downscale the texture to fit inside the max texture limit if it is too big.
    // It would be better if the image was already downscaled to the right size,
    // but this information is not always available at that time, so as a last
    // resort we can do it here. Texture coordinates are normalized, so it
    // won't cause any problems and actual texture sizes will be written
    // based on QSGTexture::textureSize which is updated after this, so that
    // should be ok.
    int max;
    if (auto rc = QSGDefaultRenderContext::from(context))
        max = rc->maxTextureSize();
    else
        funcs->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
    if (tmp.width() > max || tmp.height() > max) {
        tmp = tmp.scaled(qMin(max, tmp.width()), qMin(max, tmp.height()), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        m_texture_size = tmp.size();
    }

    // Scale to a power of two size if mipmapping is requested and the
    // texture is npot and npot textures are not properly supported.
    if (mipmapFiltering() != QSGTexture::None
        && (!isPowerOfTwo(m_texture_size.width()) || !isPowerOfTwo(m_texture_size.height()))
        && !funcs->hasOpenGLFeature(QOpenGLFunctions::NPOTTextures)) {
        tmp = tmp.scaled(qNextPowerOfTwo(m_texture_size.width()), qNextPowerOfTwo(m_texture_size.height()),
                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        m_texture_size = tmp.size();
    }

    if (tmp.width() * 4 != tmp.bytesPerLine())
        tmp = tmp.copy();

    qint64 convertTime = 0;
    if (profileFrames)
        convertTime = qsg_renderer_timer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphTexturePrepare,
                              QQuickProfiler::SceneGraphTexturePrepareConvert);

    updateBindOptions(m_dirty_bind_options);

    GLenum externalFormat = GL_RGBA;
    GLenum internalFormat = GL_RGBA;

#if defined(Q_OS_ANDROID)
    QString *deviceName =
            static_cast<QString *>(QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("AndroidDeviceName"));
    static bool wrongfullyReportsBgra8888Support = deviceName != 0
                                                    && (deviceName->compare(QLatin1String("samsung SM-T211"), Qt::CaseInsensitive) == 0
                                                        || deviceName->compare(QLatin1String("samsung SM-T210"), Qt::CaseInsensitive) == 0
                                                        || deviceName->compare(QLatin1String("samsung SM-T215"), Qt::CaseInsensitive) == 0);
#else
    static bool wrongfullyReportsBgra8888Support = false;
#endif

    if (context->hasExtension(QByteArrayLiteral("GL_EXT_bgra"))) {
        externalFormat = GL_BGRA;
#ifdef QT_OPENGL_ES
        internalFormat = GL_BGRA;
#else
        if (context->isOpenGLES())
            internalFormat = GL_BGRA;
#endif // QT_OPENGL_ES
    } else if (!wrongfullyReportsBgra8888Support
               && (context->hasExtension(QByteArrayLiteral("GL_EXT_texture_format_BGRA8888"))
                   || context->hasExtension(QByteArrayLiteral("GL_IMG_texture_format_BGRA8888")))) {
        externalFormat = GL_BGRA;
        internalFormat = GL_BGRA;
#if defined(Q_OS_DARWIN) && !defined(Q_OS_OSX)
    } else if (context->hasExtension(QByteArrayLiteral("GL_APPLE_texture_format_BGRA8888"))) {
        externalFormat = GL_BGRA;
        internalFormat = GL_RGBA;
#endif
    } else {
        qsg_swizzleBGRAToRGBA(&tmp);
    }

    qint64 swizzleTime = 0;
    if (profileFrames)
        swizzleTime = qsg_renderer_timer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphTexturePrepare,
                              QQuickProfiler::SceneGraphTexturePrepareSwizzle);

    funcs->glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_texture_size.width(), m_texture_size.height(), 0, externalFormat, GL_UNSIGNED_BYTE, tmp.constBits());

    qint64 uploadTime = 0;
    if (profileFrames)
        uploadTime = qsg_renderer_timer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphTexturePrepare,
                              QQuickProfiler::SceneGraphTexturePrepareUpload);

    if (mipmapFiltering() != QSGTexture::None) {
        funcs->glGenerateMipmap(GL_TEXTURE_2D);
        m_mipmaps_generated = true;
    }

    qint64 mipmapTime = 0;
    if (profileFrames) {
        mipmapTime = qsg_renderer_timer.nsecsElapsed();
        qCDebug(QSG_LOG_TIME_TEXTURE,
                "plain texture uploaded in: %dms (%dx%d), bind=%d, convert=%d, swizzle=%d (%s->%s), upload=%d, mipmap=%d%s",
                int(mipmapTime / 1000000),
                m_texture_size.width(), m_texture_size.height(),
                int(bindTime / 1000000),
                int((convertTime - bindTime)/1000000),
                int((swizzleTime - convertTime)/1000000),
                (externalFormat == GL_BGRA ? "BGRA" : "RGBA"),
                (internalFormat == GL_BGRA ? "BGRA" : "RGBA"),
                int((uploadTime - swizzleTime)/1000000),
                int((mipmapTime - uploadTime)/1000000),
                m_texture_size != m_image.size() ? " (scaled to GL_MAX_TEXTURE_SIZE)" : "");
    }
    Q_QUICK_SG_PROFILE_END(QQuickProfiler::SceneGraphTexturePrepare,
                           QQuickProfiler::SceneGraphTexturePrepareMipmap);

    m_texture_rect = QRectF(0, 0, 1, 1);

    m_dirty_bind_options = false;
    if (!m_retain_image)
        m_image = QImage();
#endif
}


/*!
    \class QSGDynamicTexture
    \brief The QSGDynamicTexture class serves as a baseclass for dynamically changing textures,
    such as content that is rendered to FBO's.
    \inmodule QtQuick

    To update the content of the texture, call updateTexture() explicitly. Simply calling bind()
    will not update the texture.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.
 */


/*!
    \fn bool QSGDynamicTexture::updateTexture()

    Call this function to explicitly update the dynamic texture. Calling bind() will bind
    the content that was previously updated.

    The function returns true if the texture was changed as a resul of the update; otherwise
    returns false.
 */



QT_END_NAMESPACE
