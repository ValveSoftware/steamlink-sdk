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

#ifndef QSGD3D12GLYPHCACHE_P_H
#define QSGD3D12GLYPHCACHE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtGui/private/qtextureglyphcache_p.h>

QT_BEGIN_NAMESPACE

class QSGD3D12Engine;

class QSGD3D12GlyphCache : public QTextureGlyphCache
{
public:
    QSGD3D12GlyphCache(QSGD3D12Engine *engine, QFontEngine::GlyphFormat format, const QTransform &matrix);
    ~QSGD3D12GlyphCache();

    void createTextureData(int width, int height) override;
    void resizeTextureData(int width, int height) override;
    void beginFillTexture() override;
    void fillTexture(const Coord &c, glyph_t glyph, QFixed subPixelPosition) override;
    void endFillTexture() override;
    int glyphPadding() const override;
    int maxTextureWidth() const override;
    int maxTextureHeight() const override;

    void useTexture();
    QSize currentSize() const;

private:
    QSGD3D12Engine *m_engine;
    uint m_id = 0;
    QVector<QImage> m_glyphImages;
    QVector<QPoint> m_glyphPos;
    QSize m_size;
};

QT_END_NAMESPACE

#endif // QSGD3D12GLYPHCACHE_P_H
