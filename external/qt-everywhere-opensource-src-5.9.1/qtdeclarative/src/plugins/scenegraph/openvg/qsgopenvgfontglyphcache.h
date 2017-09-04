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

#ifndef QSGOPENVGFONTGLYPHCACHE_H
#define QSGOPENVGFONTGLYPHCACHE_H

#include <QtGui/QGlyphRun>
#include <QtCore/QSet>
#include <QtCore/QLinkedList>
#include <VG/openvg.h>

QT_BEGIN_NAMESPACE

class QSGOpenVGFontGlyphCache;

class QSGOpenVGFontGlyphCacheManager
{
public:
    QSGOpenVGFontGlyphCacheManager();
    ~QSGOpenVGFontGlyphCacheManager();

    QSGOpenVGFontGlyphCache *cache(const QRawFont &font);
    void insertCache(const QRawFont &font, QSGOpenVGFontGlyphCache *cache);

private:
    QHash<QRawFont, QSGOpenVGFontGlyphCache *> m_caches;
};

class QSGOpenVGFontGlyphCache
{
public:
    QSGOpenVGFontGlyphCache(QSGOpenVGFontGlyphCacheManager *manager, const QRawFont &font);
    ~QSGOpenVGFontGlyphCache();

    const QSGOpenVGFontGlyphCacheManager *manager() const { return m_manager; }
    const QRawFont &referenceFont() const { return m_referenceFont; }
    int glyphCount() const { return m_glyphCount; }

    void populate(const QVector<quint32> &glyphs);
    void release(const QVector<quint32> &glyphs);

    VGFont font() { return m_font; }

private:
    void requestGlyphs(const QSet<quint32> &glyphs);
    void referenceGlyphs(const QSet<quint32> &glyphs);
    void releaseGlyphs(const QSet<quint32> &glyphs);

    QSGOpenVGFontGlyphCacheManager *m_manager;
    QRawFont m_referenceFont;
    int m_glyphCount;

    VGFont m_font;
    QHash<quint32, int> m_glyphReferences;
};


QT_END_NAMESPACE

#endif // QSGOPENVGFONTGLYPHCACHE_H
