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

#include "qsgopenvgfontglyphcache.h"
#include "qsgopenvghelpers.h"
#include <private/qfontengine_p.h>
#include <private/qrawfont_p.h>

QT_BEGIN_NAMESPACE

QSGOpenVGFontGlyphCacheManager::QSGOpenVGFontGlyphCacheManager()
{

}

QSGOpenVGFontGlyphCacheManager::~QSGOpenVGFontGlyphCacheManager()
{
    qDeleteAll(m_caches);
}

QSGOpenVGFontGlyphCache *QSGOpenVGFontGlyphCacheManager::cache(const QRawFont &font)
{
    return m_caches.value(font, nullptr);
}

void QSGOpenVGFontGlyphCacheManager::insertCache(const QRawFont &font, QSGOpenVGFontGlyphCache *cache)
{
    m_caches.insert(font, cache);
}

QSGOpenVGFontGlyphCache::QSGOpenVGFontGlyphCache(QSGOpenVGFontGlyphCacheManager *manager, const QRawFont &font)
    : m_manager(manager)
{
    m_referenceFont = font;
    QRawFontPrivate *fontD = QRawFontPrivate::get(font);
    m_glyphCount = fontD->fontEngine->glyphCount();
    m_font = vgCreateFont(0);
}

QSGOpenVGFontGlyphCache::~QSGOpenVGFontGlyphCache()
{
    if (m_font != VG_INVALID_HANDLE)
        vgDestroyFont(m_font);
}

void QSGOpenVGFontGlyphCache::populate(const QVector<quint32> &glyphs)
{
    QSet<quint32> referencedGlyphs;
    QSet<quint32> newGlyphs;
    int count = glyphs.count();
    for (int i = 0; i < count; ++i) {
        quint32 glyphIndex = glyphs.at(i);
        if ((int) glyphIndex >= glyphCount()) {
            qWarning("Warning: glyph is not available with index %d", glyphIndex);
            continue;
        }

        referencedGlyphs.insert(glyphIndex);


        if (!m_glyphReferences.contains(glyphIndex)) {
            newGlyphs.insert(glyphIndex);
        }
    }

    referenceGlyphs(referencedGlyphs);
    if (!newGlyphs.isEmpty())
        requestGlyphs(newGlyphs);
}

void QSGOpenVGFontGlyphCache::release(const QVector<quint32> &glyphs)
{
    QSet<quint32> unusedGlyphs;
    int count = glyphs.count();
    for (int i = 0; i < count; ++i) {
        quint32 glyphIndex = glyphs.at(i);
        unusedGlyphs.insert(glyphIndex);
    }
    releaseGlyphs(unusedGlyphs);
}

void QSGOpenVGFontGlyphCache::requestGlyphs(const QSet<quint32> &glyphs)
{
    VGfloat origin[2];
    VGfloat escapement[2];
    QRawFont rawFont = m_referenceFont;

    for (auto glyph : glyphs) {
        // Calculate the path for the glyph and cache it.
        QPainterPath path = rawFont.pathForGlyph(glyph);
        VGPath vgPath;
        if (!path.isEmpty()) {
            vgPath = QSGOpenVGHelpers::qPainterPathToVGPath(path);
        } else {
            // Probably a "space" character with no visible outline.
            vgPath = VG_INVALID_HANDLE;
        }
        origin[0] = 0;
        origin[1] = 0;
        escapement[0] = 0;
        escapement[1] = 0;
        vgSetGlyphToPath(m_font, glyph, vgPath, VG_FALSE, origin, escapement);
        vgDestroyPath(vgPath);      // Reduce reference count.
    }

}

void QSGOpenVGFontGlyphCache::referenceGlyphs(const QSet<quint32> &glyphs)
{
    for (auto glyph : glyphs) {
        if (m_glyphReferences.contains(glyph))
            m_glyphReferences[glyph] += 1;
        else
            m_glyphReferences.insert(glyph, 1);
    }
}

void QSGOpenVGFontGlyphCache::releaseGlyphs(const QSet<quint32> &glyphs)
{
    for (auto glyph : glyphs) {
        int references = m_glyphReferences[glyph] -= 1;
        if (references == 0) {
            vgClearGlyph(m_font, glyph);
            m_glyphReferences.remove(glyph);
        }
    }
}

QT_END_NAMESPACE
