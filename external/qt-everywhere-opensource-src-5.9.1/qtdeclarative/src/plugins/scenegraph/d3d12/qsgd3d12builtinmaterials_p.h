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

#ifndef QSGD3D12BUILTINMATERIALS_P_H
#define QSGD3D12BUILTINMATERIALS_P_H

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

#include "qsgd3d12material_p.h"
#include "qsgd3d12glyphcache_p.h"
#include <private/qsgtexture_p.h>
#include <QRawFont>

QT_BEGIN_NAMESPACE

class QSGD3D12RenderContext;

class QSGD3D12VertexColorMaterial : public QSGD3D12Material
{
public:
    QSGMaterialType *type() const override;
    int compare(const QSGMaterial *other) const override;

    int constantBufferSize() const override;
    void preparePipeline(QSGD3D12PipelineState *pipelineState) override;
    UpdateResults updatePipeline(const QSGD3D12MaterialRenderState &state,
                                 QSGD3D12PipelineState *pipelineState,
                                 ExtraState *extraState,
                                 quint8 *constantBuffer) override;

private:
    static QSGMaterialType mtype;
};

class QSGD3D12FlatColorMaterial : public QSGD3D12Material
{
public:
    QSGD3D12FlatColorMaterial();
    QSGMaterialType *type() const override;
    int compare(const QSGMaterial *other) const override;

    int constantBufferSize() const override;
    void preparePipeline(QSGD3D12PipelineState *pipelineState) override;
    UpdateResults updatePipeline(const QSGD3D12MaterialRenderState &state,
                                 QSGD3D12PipelineState *pipelineState,
                                 ExtraState *extraState,
                                 quint8 *constantBuffer) override;

    void setColor(const QColor &color);
    QColor color() const { return m_color; }

private:
    static QSGMaterialType mtype;
    QColor m_color;
};

class QSGD3D12SmoothColorMaterial : public QSGD3D12Material
{
public:
    QSGD3D12SmoothColorMaterial();
    QSGMaterialType *type() const override;
    int compare(const QSGMaterial *other) const override;

    int constantBufferSize() const override;
    void preparePipeline(QSGD3D12PipelineState *pipelineState) override;
    UpdateResults updatePipeline(const QSGD3D12MaterialRenderState &state,
                                 QSGD3D12PipelineState *pipelineState,
                                 ExtraState *extraState,
                                 quint8 *constantBuffer) override;

private:
    static QSGMaterialType mtype;
};

class QSGD3D12TextureMaterial : public QSGD3D12Material
{
public:
    QSGMaterialType *type() const override;
    int compare(const QSGMaterial *other) const override;

    int constantBufferSize() const override;
    void preparePipeline(QSGD3D12PipelineState *pipelineState) override;
    UpdateResults updatePipeline(const QSGD3D12MaterialRenderState &state,
                                 QSGD3D12PipelineState *pipelineState,
                                 ExtraState *extraState,
                                 quint8 *constantBuffer) override;

    void setTexture(QSGTexture *texture);
    QSGTexture *texture() const { return m_texture; }

    void setMipmapFiltering(QSGTexture::Filtering filter) { m_mipmap_filtering = filter; }
    QSGTexture::Filtering mipmapFiltering() const { return m_mipmap_filtering; }

    void setFiltering(QSGTexture::Filtering filter) { m_filtering = filter; }
    QSGTexture::Filtering filtering() const { return m_filtering; }

    void setHorizontalWrapMode(QSGTexture::WrapMode hwrap) { m_horizontal_wrap = hwrap; }
    QSGTexture::WrapMode horizontalWrapMode() const { return m_horizontal_wrap; }

    void setVerticalWrapMode(QSGTexture::WrapMode vwrap) { m_vertical_wrap = vwrap; }
    QSGTexture::WrapMode verticalWrapMode() const { return m_vertical_wrap; }

private:
    static QSGMaterialType mtype;

    QSGTexture *m_texture = nullptr;
    QSGTexture::Filtering m_filtering = QSGTexture::Nearest;
    QSGTexture::Filtering m_mipmap_filtering = QSGTexture::None;
    QSGTexture::WrapMode m_horizontal_wrap = QSGTexture::ClampToEdge;
    QSGTexture::WrapMode m_vertical_wrap = QSGTexture::ClampToEdge;
};

class QSGD3D12SmoothTextureMaterial : public QSGD3D12Material
{
public:
    QSGD3D12SmoothTextureMaterial();

    QSGMaterialType *type() const override;
    int compare(const QSGMaterial *other) const override;

    int constantBufferSize() const override;
    void preparePipeline(QSGD3D12PipelineState *pipelineState) override;
    UpdateResults updatePipeline(const QSGD3D12MaterialRenderState &state,
                                 QSGD3D12PipelineState *pipelineState,
                                 ExtraState *extraState,
                                 quint8 *constantBuffer) override;

    void setTexture(QSGTexture *texture) { m_texture = texture; }
    QSGTexture *texture() const { return m_texture; }

    void setMipmapFiltering(QSGTexture::Filtering filter) { m_mipmap_filtering = filter; }
    QSGTexture::Filtering mipmapFiltering() const { return m_mipmap_filtering; }

    void setFiltering(QSGTexture::Filtering filter) { m_filtering = filter; }
    QSGTexture::Filtering filtering() const { return m_filtering; }

    void setHorizontalWrapMode(QSGTexture::WrapMode hwrap) { m_horizontal_wrap = hwrap; }
    QSGTexture::WrapMode horizontalWrapMode() const { return m_horizontal_wrap; }

    void setVerticalWrapMode(QSGTexture::WrapMode vwrap) { m_vertical_wrap = vwrap; }
    QSGTexture::WrapMode verticalWrapMode() const { return m_vertical_wrap; }

private:
    static QSGMaterialType mtype;

    QSGTexture *m_texture = nullptr;
    QSGTexture::Filtering m_filtering = QSGTexture::Nearest;
    QSGTexture::Filtering m_mipmap_filtering = QSGTexture::None;
    QSGTexture::WrapMode m_horizontal_wrap = QSGTexture::ClampToEdge;
    QSGTexture::WrapMode m_vertical_wrap = QSGTexture::ClampToEdge;
};

class QSGD3D12TextMaterial : public QSGD3D12Material
{
public:
    enum StyleType {
        Normal,
        Styled,
        Outlined,

        NStyleTypes
    };
    QSGD3D12TextMaterial(StyleType styleType, QSGD3D12RenderContext *rc, const QRawFont &font,
                         QFontEngine::GlyphFormat glyphFormat = QFontEngine::Format_None);

    QSGMaterialType *type() const override;
    int compare(const QSGMaterial *other) const override;

    int constantBufferSize() const override;
    void preparePipeline(QSGD3D12PipelineState *pipelineState) override;
    UpdateResults updatePipeline(const QSGD3D12MaterialRenderState &state,
                                 QSGD3D12PipelineState *pipelineState,
                                 ExtraState *extraState,
                                 quint8 *constantBuffer) override;

    void setColor(const QColor &c) { m_color = QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()); }
    void setColor(const QVector4D &color) { m_color = color; }
    const QVector4D &color() const { return m_color; }

    void setStyleShift(const QVector2D &shift) { m_styleShift = shift; }
    const QVector2D &styleShift() const { return m_styleShift; }

    void setStyleColor(const QColor &c) { m_styleColor = QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()); }
    void setStyleColor(const QVector4D &color) { m_styleColor = color; }
    const QVector4D &styleColor() const { return m_styleColor; }

    void populate(const QPointF &position,
                  const QVector<quint32> &glyphIndexes, const QVector<QPointF> &glyphPositions,
                  QSGGeometry *geometry, QRectF *boundingRect, QPointF *baseLine,
                  const QMargins &margins = QMargins(0, 0, 0, 0));

    QSGD3D12GlyphCache *glyphCache() const { return static_cast<QSGD3D12GlyphCache *>(m_glyphCache.data()); }

private:
    static const int NTextMaterialTypes = NStyleTypes * 2;
    static QSGMaterialType mtype[NTextMaterialTypes];
    StyleType m_styleType;
    QSGD3D12RenderContext *m_rc;
    QVector4D m_color;
    QVector2D m_styleShift;
    QVector4D m_styleColor;
    QRawFont m_font;
    QExplicitlySharedDataPointer<QFontEngineGlyphCache> m_glyphCache;
};

QT_END_NAMESPACE

#endif // QSGD3D12BUILTINMATERIALS_P_H
