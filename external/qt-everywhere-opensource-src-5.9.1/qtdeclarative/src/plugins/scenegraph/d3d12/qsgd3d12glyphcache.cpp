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

#include "qsgd3d12glyphcache_p.h"
#include "qsgd3d12engine_p.h"

QT_BEGIN_NAMESPACE

// Convert A8 glyphs to 32-bit in the engine. This is here to work around
// QTBUG-55330 for AMD cards.
// If removing, textmask.hlsl must be adjusted! (.a -> .r)
#define ALWAYS_32BIT

// NOTE: Avoid categorized logging. It is slow.

#define DECLARE_DEBUG_VAR(variable) \
    static bool debug_ ## variable() \
    { static bool value = qgetenv("QSG_RENDERER_DEBUG").contains(QT_STRINGIFY(variable)); return value; }

DECLARE_DEBUG_VAR(render)

QSGD3D12GlyphCache::QSGD3D12GlyphCache(QSGD3D12Engine *engine, QFontEngine::GlyphFormat format, const QTransform &matrix)
    : QTextureGlyphCache(format, matrix),
      m_engine(engine)
{
}

QSGD3D12GlyphCache::~QSGD3D12GlyphCache()
{
    if (m_id)
        m_engine->releaseTexture(m_id);
}

void QSGD3D12GlyphCache::createTextureData(int width, int height)
{
    width = qMax(128, width);
    height = qMax(32, height);

    m_id = m_engine->genTexture();
    Q_ASSERT(m_id);

    if (Q_UNLIKELY(debug_render()))
        qDebug("new glyph cache texture %u of size %dx%d, fontengine format %d", m_id, width, height, m_format);

    m_size = QSize(width, height);

    const QImage::Format imageFormat =
            m_format == QFontEngine::Format_A8 ? QImage::Format_Alpha8 : QImage::Format_ARGB32_Premultiplied;
    m_engine->createTexture(m_id, m_size, imageFormat, QSGD3D12Engine::TextureWithAlpha
#ifdef ALWAYS_32BIT
                            | QSGD3D12Engine::TextureAlways32Bit
#endif
                            );
}

void QSGD3D12GlyphCache::resizeTextureData(int width, int height)
{
    width = qMax(128, width);
    height = qMax(32, height);

    if (m_size.width() >= width && m_size.height() >= height)
        return;

    if (Q_UNLIKELY(debug_render()))
        qDebug("glyph cache texture %u resize to %dx%d", m_id, width, height);

    m_size = QSize(width, height);

    m_engine->queueTextureResize(m_id, m_size);
}

void QSGD3D12GlyphCache::beginFillTexture()
{
    Q_ASSERT(m_glyphImages.isEmpty() && m_glyphPos.isEmpty());
}

void QSGD3D12GlyphCache::fillTexture(const Coord &c, glyph_t glyph, QFixed subPixelPosition)
{
    QImage mask = textureMapForGlyph(glyph, subPixelPosition);
    const int maskWidth = mask.width();
    const int maskHeight = mask.height();

    if (mask.format() == QImage::Format_Mono) {
        mask = mask.convertToFormat(QImage::Format_Indexed8);
        for (int y = 0; y < maskHeight; ++y) {
            uchar *src = mask.scanLine(y);
            for (int x = 0; x < maskWidth; ++x)
                src[x] = -src[x]; // convert 0 and 1 into 0 and 255
        }
    } else if (mask.depth() == 32) {
        if (mask.format() == QImage::Format_RGB32) {
            // We need to make the alpha component equal to the average of the RGB values.
            // This is needed when drawing sub-pixel antialiased text on translucent targets.
            for (int y = 0; y < maskHeight; ++y) {
                QRgb *src = reinterpret_cast<QRgb *>(mask.scanLine(y));
                for (int x = 0; x < maskWidth; ++x) {
                    const int r = qRed(src[x]);
                    const int g = qGreen(src[x]);
                    const int b = qBlue(src[x]);
                    int avg;
                    if (mask.format() == QImage::Format_RGB32)
                        avg = (r + g + b + 1) / 3; // "+1" for rounding.
                    else // Format_ARGB32_Premultiplied
                        avg = qAlpha(src[x]);
                    src[x] = qRgba(r, g, b, avg);
                }
            }
        }
    }

    m_glyphImages.append(mask);
    m_glyphPos.append(QPoint(c.x, c.y));
}

void QSGD3D12GlyphCache::endFillTexture()
{
    if (m_glyphImages.isEmpty())
        return;

    Q_ASSERT(m_id);

    m_engine->queueTextureUpload(m_id, m_glyphImages, m_glyphPos
#ifdef ALWAYS_32BIT
                                 , QSGD3D12Engine::TextureUploadAlways32Bit
#endif
                                 );

    // Nothing else left to do, it is up to the text material to call
    // useTexture() which will then add the texture dependency to the frame.

    m_glyphImages.clear();
    m_glyphPos.clear();
}

int QSGD3D12GlyphCache::glyphPadding() const
{
    return 1;
}

int QSGD3D12GlyphCache::maxTextureWidth() const
{
    return 16384;
}

int QSGD3D12GlyphCache::maxTextureHeight() const
{
    return 16384;
}

void QSGD3D12GlyphCache::useTexture()
{
    if (m_id)
        m_engine->useTexture(m_id);
}

QSize QSGD3D12GlyphCache::currentSize() const
{
    return m_size;
}

QT_END_NAMESPACE
