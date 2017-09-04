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

#ifndef QSGOPENVGGLYPHNODE_H
#define QSGOPENVGGLYPHNODE_H

#include <private/qsgadaptationlayer_p.h>
#include <QtCore/QVector>

#include <VG/openvg.h>

#include "qsgopenvgrenderable.h"
#include "qsgopenvgfontglyphcache.h"

QT_BEGIN_NAMESPACE

class QSGOpenVGFontGlyphCache;
class QSGOpenVGRenderContext;
class QSGRenderContext;

class QSGOpenVGGlyphNode : public QSGGlyphNode, public QSGOpenVGRenderable
{
public:
    QSGOpenVGGlyphNode(QSGRenderContext *rc);
    ~QSGOpenVGGlyphNode();

    void setGlyphs(const QPointF &position, const QGlyphRun &glyphs) override;
    void setColor(const QColor &color) override;
    void setStyle(QQuickText::TextStyle style) override;
    void setStyleColor(const QColor &color) override;
    QPointF baseLine() const override;
    void setPreferredAntialiasingMode(AntialiasingMode) override;
    void update() override;

    void render() override;
    void setOpacity(float opacity) override;

private:
    void drawGlyphsAtOffset(const QPointF &offset);

    QPointF m_position;
    QGlyphRun m_glyphRun;
    QColor m_color;
    QSGGeometry m_geometry;
    QQuickText::TextStyle m_style;
    QColor m_styleColor;

    QSGOpenVGFontGlyphCache *m_glyphCache;
    QVector<VGfloat> m_xAdjustments;
    QVector<VGfloat> m_yAdjustments;
    VGPaint m_fontColorPaint;
    VGPaint m_styleColorPaint;

    QSGOpenVGRenderContext *m_renderContext;
};

QT_END_NAMESPACE

#endif // QSGOPENVGGLYPHNODE_H
