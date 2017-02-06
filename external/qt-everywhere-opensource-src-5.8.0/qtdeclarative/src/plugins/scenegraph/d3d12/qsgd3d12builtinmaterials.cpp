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

#include "qsgd3d12builtinmaterials_p.h"
#include "qsgd3d12rendercontext_p.h"
#include <QQuickWindow>
#include <QtCore/qmath.h>
#include <QtGui/private/qfixed_p.h>

#include "vs_vertexcolor.hlslh"
#include "ps_vertexcolor.hlslh"
#include "vs_flatcolor.hlslh"
#include "ps_flatcolor.hlslh"
#include "vs_smoothcolor.hlslh"
#include "ps_smoothcolor.hlslh"
#include "vs_texture.hlslh"
#include "ps_texture.hlslh"
#include "vs_smoothtexture.hlslh"
#include "ps_smoothtexture.hlslh"
#include "vs_textmask.hlslh"
#include "ps_textmask24.hlslh"
#include "ps_textmask32.hlslh"
#include "ps_textmask8.hlslh"
#include "vs_styledtext.hlslh"
#include "ps_styledtext.hlslh"
#include "vs_outlinedtext.hlslh"
#include "ps_outlinedtext.hlslh"

QT_BEGIN_NAMESPACE

// NB! In HLSL constant buffer data is packed into 4-byte boundaries and, more
// importantly, it is packed so that it does not cross a 16-byte (float4)
// boundary. Hence the need for padding in some cases.

static inline QVector4D qsg_premultiply(const QVector4D &c, float globalOpacity)
{
    const float o = c.w() * globalOpacity;
    return QVector4D(c.x() * o, c.y() * o, c.z() * o, o);
}

static inline QVector4D qsg_premultiply(const QColor &c, float globalOpacity)
{
    const float o = c.alphaF() * globalOpacity;
    return QVector4D(c.redF() * o, c.greenF() * o, c.blueF() * o, o);
}

static inline int qsg_colorDiff(const QVector4D &a, const QVector4D &b)
{
    if (a.x() != b.x())
        return a.x() > b.x() ? 1 : -1;
    if (a.y() != b.y())
        return a.y() > b.y() ? 1 : -1;
    if (a.z() != b.z())
        return a.z() > b.z() ? 1 : -1;
    if (a.w() != b.w())
        return a.w() > b.w() ? 1 : -1;
    return 0;
}

QSGMaterialType QSGD3D12VertexColorMaterial::mtype;

QSGMaterialType *QSGD3D12VertexColorMaterial::type() const
{
    return &QSGD3D12VertexColorMaterial::mtype;
}

int QSGD3D12VertexColorMaterial::compare(const QSGMaterial *other) const
{
    Q_UNUSED(other);
    Q_ASSERT(other && type() == other->type());
    // As the vertex color material has all its state in the vertex attributes
    // defined by the geometry, all such materials will be equal.
    return 0;
}

static const int VERTEX_COLOR_CB_SIZE_0 = 16 * sizeof(float); // float4x4
static const int VERTEX_COLOR_CB_SIZE_1 = sizeof(float); // float
static const int VERTEX_COLOR_CB_SIZE = VERTEX_COLOR_CB_SIZE_0 + VERTEX_COLOR_CB_SIZE_1;

int QSGD3D12VertexColorMaterial::constantBufferSize() const
{
    return QSGD3D12Engine::alignedConstantBufferSize(VERTEX_COLOR_CB_SIZE);
}

void QSGD3D12VertexColorMaterial::preparePipeline(QSGD3D12PipelineState *pipelineState)
{
    pipelineState->shaders.vs = g_VS_VertexColor;
    pipelineState->shaders.vsSize = sizeof(g_VS_VertexColor);
    pipelineState->shaders.ps = g_PS_VertexColor;
    pipelineState->shaders.psSize = sizeof(g_PS_VertexColor);
}

QSGD3D12Material::UpdateResults QSGD3D12VertexColorMaterial::updatePipeline(const QSGD3D12MaterialRenderState &state,
                                                                            QSGD3D12PipelineState *,
                                                                            ExtraState *,
                                                                            quint8 *constantBuffer)
{
    QSGD3D12Material::UpdateResults r = 0;
    quint8 *p = constantBuffer;

    if (state.isMatrixDirty()) {
        memcpy(p, state.combinedMatrix().constData(), VERTEX_COLOR_CB_SIZE_0);
        r |= UpdatedConstantBuffer;
    }
    p += VERTEX_COLOR_CB_SIZE_0;

    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        memcpy(p, &opacity, VERTEX_COLOR_CB_SIZE_1);
        r |= UpdatedConstantBuffer;
    }

    return r;
}

QSGD3D12FlatColorMaterial::QSGD3D12FlatColorMaterial()
    : m_color(QColor(255, 255, 255))
{
}

QSGMaterialType QSGD3D12FlatColorMaterial::mtype;

QSGMaterialType *QSGD3D12FlatColorMaterial::type() const
{
    return &QSGD3D12FlatColorMaterial::mtype;
}

int QSGD3D12FlatColorMaterial::compare(const QSGMaterial *other) const
{
    Q_ASSERT(other && type() == other->type());
    const QSGD3D12FlatColorMaterial *o = static_cast<const QSGD3D12FlatColorMaterial *>(other);
    return m_color.rgba() - o->color().rgba();
}

static const int FLAT_COLOR_CB_SIZE_0 = 16 * sizeof(float); // float4x4
static const int FLAT_COLOR_CB_SIZE_1 = 4 * sizeof(float); // float4
static const int FLAT_COLOR_CB_SIZE = FLAT_COLOR_CB_SIZE_0 + FLAT_COLOR_CB_SIZE_1;

int QSGD3D12FlatColorMaterial::constantBufferSize() const
{
    return QSGD3D12Engine::alignedConstantBufferSize(FLAT_COLOR_CB_SIZE);
}

void QSGD3D12FlatColorMaterial::preparePipeline(QSGD3D12PipelineState *pipelineState)
{
    pipelineState->shaders.vs = g_VS_FlatColor;
    pipelineState->shaders.vsSize = sizeof(g_VS_FlatColor);
    pipelineState->shaders.ps = g_PS_FlatColor;
    pipelineState->shaders.psSize = sizeof(g_PS_FlatColor);
}

QSGD3D12Material::UpdateResults QSGD3D12FlatColorMaterial::updatePipeline(const QSGD3D12MaterialRenderState &state,
                                                                          QSGD3D12PipelineState *,
                                                                          ExtraState *,
                                                                          quint8 *constantBuffer)
{
    QSGD3D12Material::UpdateResults r = 0;
    quint8 *p = constantBuffer;

    if (state.isMatrixDirty()) {
        memcpy(p, state.combinedMatrix().constData(), FLAT_COLOR_CB_SIZE_0);
        r |= UpdatedConstantBuffer;
    }
    p += FLAT_COLOR_CB_SIZE_0;

    const QVector4D color = qsg_premultiply(m_color, state.opacity());
    const float f[] = { color.x(), color.y(), color.z(), color.w() };
    if (state.isOpacityDirty() || memcmp(p, f, FLAT_COLOR_CB_SIZE_1)) {
        memcpy(p, f, FLAT_COLOR_CB_SIZE_1);
        r |= UpdatedConstantBuffer;
    }

    return r;
}

void QSGD3D12FlatColorMaterial::setColor(const QColor &color)
{
    m_color = color;
    setFlag(Blending, m_color.alpha() != 0xFF);
}

QSGD3D12SmoothColorMaterial::QSGD3D12SmoothColorMaterial()
{
    setFlag(RequiresFullMatrixExceptTranslate, true);
    setFlag(Blending, true);
}

QSGMaterialType QSGD3D12SmoothColorMaterial::mtype;

QSGMaterialType *QSGD3D12SmoothColorMaterial::type() const
{
    return &QSGD3D12SmoothColorMaterial::mtype;
}

int QSGD3D12SmoothColorMaterial::compare(const QSGMaterial *other) const
{
    Q_UNUSED(other);
    Q_ASSERT(other && type() == other->type());
    return 0;
}

static const int SMOOTH_COLOR_CB_SIZE_0 = 16 * sizeof(float); // float4x4
static const int SMOOTH_COLOR_CB_SIZE_1 = sizeof(float); // float
static const int SMOOTH_COLOR_CB_SIZE_2 = 2 * sizeof(float); // float2
static const int SMOOTH_COLOR_CB_SIZE = SMOOTH_COLOR_CB_SIZE_0 + SMOOTH_COLOR_CB_SIZE_1 + SMOOTH_COLOR_CB_SIZE_2;

int QSGD3D12SmoothColorMaterial::constantBufferSize() const
{
    return QSGD3D12Engine::alignedConstantBufferSize(SMOOTH_COLOR_CB_SIZE);
}

void QSGD3D12SmoothColorMaterial::preparePipeline(QSGD3D12PipelineState *pipelineState)
{
    pipelineState->shaders.vs = g_VS_SmoothColor;
    pipelineState->shaders.vsSize = sizeof(g_VS_SmoothColor);
    pipelineState->shaders.ps = g_PS_SmoothColor;
    pipelineState->shaders.psSize = sizeof(g_PS_SmoothColor);
}

QSGD3D12Material::UpdateResults QSGD3D12SmoothColorMaterial::updatePipeline(const QSGD3D12MaterialRenderState &state,
                                                                            QSGD3D12PipelineState *,
                                                                            ExtraState *,
                                                                            quint8 *constantBuffer)
{
    QSGD3D12Material::UpdateResults r = 0;
    quint8 *p = constantBuffer;

    if (state.isMatrixDirty()) {
        memcpy(p, state.combinedMatrix().constData(), SMOOTH_COLOR_CB_SIZE_0);
        r |= UpdatedConstantBuffer;
    }
    p += SMOOTH_COLOR_CB_SIZE_0;

    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        memcpy(p, &opacity, SMOOTH_COLOR_CB_SIZE_1);
        r |= UpdatedConstantBuffer;
    }
    p += SMOOTH_COLOR_CB_SIZE_1;

    if (state.isMatrixDirty()) {
        const QRect viewport = state.viewportRect();
        const float v[] = { 2.0f / viewport.width(), 2.0f / viewport.height() };
        memcpy(p, v, SMOOTH_COLOR_CB_SIZE_2);
        r |= UpdatedConstantBuffer;
    }

    return r;
}

QSGMaterialType QSGD3D12TextureMaterial::mtype;

QSGMaterialType *QSGD3D12TextureMaterial::type() const
{
    return &QSGD3D12TextureMaterial::mtype;
}

int QSGD3D12TextureMaterial::compare(const QSGMaterial *other) const
{
    Q_ASSERT(other && type() == other->type());
    const QSGD3D12TextureMaterial *o = static_cast<const QSGD3D12TextureMaterial *>(other);
    if (int diff = m_texture->textureId() - o->texture()->textureId())
        return diff;
    return int(m_filtering) - int(o->m_filtering);
}

static const int TEXTURE_CB_SIZE_0 = 16 * sizeof(float); // float4x4
static const int TEXTURE_CB_SIZE_1 = sizeof(float); // float
static const int TEXTURE_CB_SIZE = TEXTURE_CB_SIZE_0 + TEXTURE_CB_SIZE_1;

int QSGD3D12TextureMaterial::constantBufferSize() const
{
    return QSGD3D12Engine::alignedConstantBufferSize(TEXTURE_CB_SIZE);
}

void QSGD3D12TextureMaterial::preparePipeline(QSGD3D12PipelineState *pipelineState)
{
    pipelineState->shaders.vs = g_VS_Texture;
    pipelineState->shaders.vsSize = sizeof(g_VS_Texture);
    pipelineState->shaders.ps = g_PS_Texture;
    pipelineState->shaders.psSize = sizeof(g_PS_Texture);

    pipelineState->shaders.rootSig.textureViewCount = 1;
}

QSGD3D12Material::UpdateResults QSGD3D12TextureMaterial::updatePipeline(const QSGD3D12MaterialRenderState &state,
                                                                        QSGD3D12PipelineState *pipelineState,
                                                                        ExtraState *,
                                                                        quint8 *constantBuffer)
{
    QSGD3D12Material::UpdateResults r = 0;
    quint8 *p = constantBuffer;

    if (state.isMatrixDirty()) {
        memcpy(p, state.combinedMatrix().constData(), TEXTURE_CB_SIZE_0);
        r |= UpdatedConstantBuffer;
    }
    p += TEXTURE_CB_SIZE_0;

    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        memcpy(p, &opacity, TEXTURE_CB_SIZE_1);
        r |= UpdatedConstantBuffer;
    }

    Q_ASSERT(m_texture);
    m_texture->setFiltering(m_filtering);
    m_texture->setMipmapFiltering(m_mipmap_filtering);
    m_texture->setHorizontalWrapMode(m_horizontal_wrap);
    m_texture->setVerticalWrapMode(m_vertical_wrap);

    QSGD3D12TextureView &tv(pipelineState->shaders.rootSig.textureViews[0]);
    if (m_filtering == QSGTexture::Linear)
        tv.filter = m_mipmap_filtering == QSGTexture::Linear
                ? QSGD3D12TextureView::FilterLinear : QSGD3D12TextureView::FilterMinMagLinearMipNearest;
    else
        tv.filter = m_mipmap_filtering == QSGTexture::Linear
                ? QSGD3D12TextureView::FilterMinMagNearestMipLinear : QSGD3D12TextureView::FilterNearest;
    tv.addressModeHoriz = m_horizontal_wrap == QSGTexture::ClampToEdge ? QSGD3D12TextureView::AddressClamp : QSGD3D12TextureView::AddressWrap;
    tv.addressModeVert = m_vertical_wrap == QSGTexture::ClampToEdge ? QSGD3D12TextureView::AddressClamp : QSGD3D12TextureView::AddressWrap;

    m_texture->bind();

    return r;
}

void QSGD3D12TextureMaterial::setTexture(QSGTexture *texture)
{
    m_texture = texture;
    setFlag(Blending, m_texture ? m_texture->hasAlphaChannel() : false);
}

QSGD3D12SmoothTextureMaterial::QSGD3D12SmoothTextureMaterial()
{
    setFlag(RequiresFullMatrixExceptTranslate, true);
    setFlag(Blending, true);
}

QSGMaterialType QSGD3D12SmoothTextureMaterial::mtype;

QSGMaterialType *QSGD3D12SmoothTextureMaterial::type() const
{
    return &QSGD3D12SmoothTextureMaterial::mtype;
}

int QSGD3D12SmoothTextureMaterial::compare(const QSGMaterial *other) const
{
    Q_ASSERT(other && type() == other->type());
    const QSGD3D12SmoothTextureMaterial *o = static_cast<const QSGD3D12SmoothTextureMaterial *>(other);
    if (int diff = m_texture->textureId() - o->texture()->textureId())
        return diff;
    return int(m_filtering) - int(o->m_filtering);
}

static const int SMOOTH_TEXTURE_CB_SIZE_0 = 16 * sizeof(float); // float4x4
static const int SMOOTH_TEXTURE_CB_SIZE_1 = sizeof(float); // float
static const int SMOOTH_TEXTURE_CB_SIZE_2 = 2 * sizeof(float); // float2
static const int SMOOTH_TEXTURE_CB_SIZE = SMOOTH_TEXTURE_CB_SIZE_0 + SMOOTH_TEXTURE_CB_SIZE_1 + SMOOTH_TEXTURE_CB_SIZE_2;

int QSGD3D12SmoothTextureMaterial::constantBufferSize() const
{
    return QSGD3D12Engine::alignedConstantBufferSize(SMOOTH_TEXTURE_CB_SIZE);
}

void QSGD3D12SmoothTextureMaterial::preparePipeline(QSGD3D12PipelineState *pipelineState)
{
    pipelineState->shaders.vs = g_VS_SmoothTexture;
    pipelineState->shaders.vsSize = sizeof(g_VS_SmoothTexture);
    pipelineState->shaders.ps = g_PS_SmoothTexture;
    pipelineState->shaders.psSize = sizeof(g_PS_SmoothTexture);

    pipelineState->shaders.rootSig.textureViewCount = 1;
}

QSGD3D12Material::UpdateResults QSGD3D12SmoothTextureMaterial::updatePipeline(const QSGD3D12MaterialRenderState &state,
                                                                              QSGD3D12PipelineState *pipelineState,
                                                                              ExtraState *,
                                                                              quint8 *constantBuffer)
{
    QSGD3D12Material::UpdateResults r = 0;
    quint8 *p = constantBuffer;

    if (state.isMatrixDirty()) {
        memcpy(p, state.combinedMatrix().constData(), SMOOTH_TEXTURE_CB_SIZE_0);
        r |= UpdatedConstantBuffer;
    }
    p += SMOOTH_TEXTURE_CB_SIZE_0;

    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        memcpy(p, &opacity, SMOOTH_TEXTURE_CB_SIZE_1);
        r |= UpdatedConstantBuffer;
    }
    p += SMOOTH_TEXTURE_CB_SIZE_1;

    if (state.isMatrixDirty()) {
        const QRect viewport = state.viewportRect();
        const float v[] = { 2.0f / viewport.width(), 2.0f / viewport.height() };
        memcpy(p, v, SMOOTH_TEXTURE_CB_SIZE_2);
        r |= UpdatedConstantBuffer;
    }

    Q_ASSERT(m_texture);
    m_texture->setFiltering(m_filtering);
    m_texture->setMipmapFiltering(m_mipmap_filtering);
    m_texture->setHorizontalWrapMode(m_horizontal_wrap);
    m_texture->setVerticalWrapMode(m_vertical_wrap);

    QSGD3D12TextureView &tv(pipelineState->shaders.rootSig.textureViews[0]);
    if (m_filtering == QSGTexture::Linear)
        tv.filter = m_mipmap_filtering == QSGTexture::Linear
                ? QSGD3D12TextureView::FilterLinear : QSGD3D12TextureView::FilterMinMagLinearMipNearest;
    else
        tv.filter = m_mipmap_filtering == QSGTexture::Linear
                ? QSGD3D12TextureView::FilterMinMagNearestMipLinear : QSGD3D12TextureView::FilterNearest;
    tv.addressModeHoriz = m_horizontal_wrap == QSGTexture::ClampToEdge ? QSGD3D12TextureView::AddressClamp : QSGD3D12TextureView::AddressWrap;
    tv.addressModeVert = m_vertical_wrap == QSGTexture::ClampToEdge ? QSGD3D12TextureView::AddressClamp : QSGD3D12TextureView::AddressWrap;

    m_texture->bind();

    return r;
}

QSGD3D12TextMaterial::QSGD3D12TextMaterial(StyleType styleType, QSGD3D12RenderContext *rc,
                                           const QRawFont &font, QFontEngine::GlyphFormat glyphFormat)
    : m_styleType(styleType),
      m_font(font),
      m_rc(rc)
{
    setFlag(Blending, true);

    QRawFontPrivate *fontD = QRawFontPrivate::get(m_font);
    if (QFontEngine *fontEngine = fontD->fontEngine) {
        if (glyphFormat == QFontEngine::Format_None)
            glyphFormat = fontEngine->glyphFormat != QFontEngine::Format_None
                    ? fontEngine->glyphFormat : QFontEngine::Format_A32;

        QSGD3D12Engine *d3dengine = rc->engine();
        const float devicePixelRatio = d3dengine->windowDevicePixelRatio();
        QTransform glyphCacheTransform = QTransform::fromScale(devicePixelRatio, devicePixelRatio);
        if (!fontEngine->supportsTransformation(glyphCacheTransform))
            glyphCacheTransform = QTransform();

        m_glyphCache = fontEngine->glyphCache(d3dengine, glyphFormat, glyphCacheTransform);
        if (!m_glyphCache || int(m_glyphCache->glyphFormat()) != glyphFormat) {
            m_glyphCache = new QSGD3D12GlyphCache(d3dengine, glyphFormat, glyphCacheTransform);
            fontEngine->setGlyphCache(d3dengine, m_glyphCache.data());
            rc->registerFontengineForCleanup(fontEngine);
        }
    }
}

QSGMaterialType QSGD3D12TextMaterial::mtype[QSGD3D12TextMaterial::NTextMaterialTypes];

QSGMaterialType *QSGD3D12TextMaterial::type() const
{
    // Format_A32 has special blend settings and therefore two materials with
    // the same style but different formats where one is A32 are treated as
    // different. This way the renderer can manage the pipeline state properly.
    const int matStyle = m_styleType * 2;
    const int matFormat = glyphCache()->glyphFormat() != QFontEngine::Format_A32 ? 0 : 1;
    return &QSGD3D12TextMaterial::mtype[matStyle + matFormat];
}

int QSGD3D12TextMaterial::compare(const QSGMaterial *other) const
{
    Q_ASSERT(other && type() == other->type());
    const QSGD3D12TextMaterial *o = static_cast<const QSGD3D12TextMaterial *>(other);
    if (m_styleType != o->m_styleType)
        return m_styleType - o->m_styleType;
    if (m_glyphCache != o->m_glyphCache)
        return m_glyphCache.data() < o->m_glyphCache.data() ? -1 : 1;
    if (m_styleShift != o->m_styleShift)
        return m_styleShift.y() - o->m_styleShift.y();
    int styleColorDiff = qsg_colorDiff(m_styleColor, o->m_styleColor);
    if (styleColorDiff)
        return styleColorDiff;
    return qsg_colorDiff(m_color, o->m_color);
}

static const int TEXT_CB_SIZE_0 = 16 * sizeof(float); // float4x4 mvp
static const int TEXT_CB_SIZE_1 = 2 * sizeof(float); // float2 textureScale
static const int TEXT_CB_SIZE_2 = sizeof(float); // float dpr
static const int TEXT_CB_SIZE_3 = sizeof(float); // float color
static const int TEXT_CB_SIZE_4 = 4 * sizeof(float); // float4 colorVec
static const int TEXT_CB_SIZE_5 = 2 * sizeof(float); // float2 shift
static const int TEXT_CB_SIZE_5_PADDING = 2 * sizeof(float); // float2 padding (the next float4 would cross the 16-byte boundary)
static const int TEXT_CB_SIZE_6 = 4 * sizeof(float); // float4 styleColor
static const int TEXT_CB_SIZE = TEXT_CB_SIZE_0 + TEXT_CB_SIZE_1 + TEXT_CB_SIZE_2 + TEXT_CB_SIZE_3
        + TEXT_CB_SIZE_4 + TEXT_CB_SIZE_5 + TEXT_CB_SIZE_5_PADDING + TEXT_CB_SIZE_6;

int QSGD3D12TextMaterial::constantBufferSize() const
{
    return QSGD3D12Engine::alignedConstantBufferSize(TEXT_CB_SIZE);
}

void QSGD3D12TextMaterial::preparePipeline(QSGD3D12PipelineState *pipelineState)
{
    if (m_styleType == Normal) {
        pipelineState->shaders.vs = g_VS_TextMask;
        pipelineState->shaders.vsSize = sizeof(g_VS_TextMask);
        switch (glyphCache()->glyphFormat()) {
        case QFontEngine::Format_A32:
            pipelineState->shaders.ps = g_PS_TextMask24;
            pipelineState->shaders.psSize = sizeof(g_PS_TextMask24);
            break;
        case QFontEngine::Format_ARGB:
            pipelineState->shaders.ps = g_PS_TextMask32;
            pipelineState->shaders.psSize = sizeof(g_PS_TextMask32);
            break;
        default:
            pipelineState->shaders.ps = g_PS_TextMask8;
            pipelineState->shaders.psSize = sizeof(g_PS_TextMask8);
            break;
        }
    } else if (m_styleType == Outlined) {
        pipelineState->shaders.vs = g_VS_OutlinedText;
        pipelineState->shaders.vsSize = sizeof(g_VS_OutlinedText);
        pipelineState->shaders.ps = g_PS_OutlinedText;
        pipelineState->shaders.psSize = sizeof(g_PS_OutlinedText);
    } else {
        pipelineState->shaders.vs = g_VS_StyledText;
        pipelineState->shaders.vsSize = sizeof(g_VS_StyledText);
        pipelineState->shaders.ps = g_PS_StyledText;
        pipelineState->shaders.psSize = sizeof(g_PS_StyledText);
    }

    pipelineState->shaders.rootSig.textureViewCount = 1;
}

QSGD3D12Material::UpdateResults QSGD3D12TextMaterial::updatePipeline(const QSGD3D12MaterialRenderState &state,
                                                                     QSGD3D12PipelineState *pipelineState,
                                                                     ExtraState *extraState,
                                                                     quint8 *constantBuffer)
{
    QSGD3D12Material::UpdateResults r = 0;
    quint8 *p = constantBuffer;

    if (glyphCache()->glyphFormat() == QFontEngine::Format_A32) {
        // can freely change the state due to the way type() works
        pipelineState->blend = QSGD3D12PipelineState::BlendColor;
        extraState->blendFactor = m_color;
        r |= UpdatedBlendFactor; // must be set always as this affects the command list
    }

    if (state.isMatrixDirty()) {
        memcpy(p, state.combinedMatrix().constData(), TEXT_CB_SIZE_0);
        r |= UpdatedConstantBuffer;
    }
    p += TEXT_CB_SIZE_0;

    const QSize sz = glyphCache()->currentSize();
    const float textureScale[] = { 1.0f / sz.width(), 1.0f / sz.height() };
    if (state.isCachedMaterialDataDirty() || memcmp(p, textureScale, TEXT_CB_SIZE_1)) {
        memcpy(p, textureScale, TEXT_CB_SIZE_1);
        r |= UpdatedConstantBuffer;
    }
    p += TEXT_CB_SIZE_1;

    const float dpr = m_rc->engine()->windowDevicePixelRatio();
    if (state.isCachedMaterialDataDirty() || memcmp(p, &dpr, TEXT_CB_SIZE_2)) {
        memcpy(p, &dpr, TEXT_CB_SIZE_2);
        r |= UpdatedConstantBuffer;
    }
    p += TEXT_CB_SIZE_2;

    if (glyphCache()->glyphFormat() == QFontEngine::Format_A32) {
        const QVector4D color = qsg_premultiply(m_color, state.opacity());
        const float alpha = color.w();
        if (state.isOpacityDirty() || memcmp(p, &alpha, TEXT_CB_SIZE_3)) {
            memcpy(p, &alpha, TEXT_CB_SIZE_3);
            r |= UpdatedConstantBuffer;
        }
    } else if (glyphCache()->glyphFormat() == QFontEngine::Format_ARGB) {
        const float opacity = m_color.w() * state.opacity();
        if (state.isOpacityDirty() || memcmp(p, &opacity, TEXT_CB_SIZE_3)) {
            memcpy(p, &opacity, TEXT_CB_SIZE_3);
            r |= UpdatedConstantBuffer;
        }
    } else {
        const QVector4D color = qsg_premultiply(m_color, state.opacity());
        const float f[] = { color.x(), color.y(), color.z(), color.w() };
        if (state.isOpacityDirty() || memcmp(p, f, TEXT_CB_SIZE_4)) {
            memcpy(p + TEXT_CB_SIZE_3, f, TEXT_CB_SIZE_4);
            r |= UpdatedConstantBuffer;
        }
    }
    p += TEXT_CB_SIZE_3 + TEXT_CB_SIZE_4;

    if (m_styleType == Styled) {
        const float f[] = { m_styleShift.x(), m_styleShift.y() };
        if (state.isCachedMaterialDataDirty() || memcmp(p, f, TEXT_CB_SIZE_5)) {
            memcpy(p, f, TEXT_CB_SIZE_5);
            r |= UpdatedConstantBuffer;
        }
    }
    p += TEXT_CB_SIZE_5 + TEXT_CB_SIZE_5_PADDING;

    if (m_styleType == Styled || m_styleType == Outlined) {
        const QVector4D color = qsg_premultiply(m_styleColor, state.opacity());
        const float f[] = { color.x(), color.y(), color.z(), color.w() };
        if (state.isOpacityDirty() || memcmp(p, f, TEXT_CB_SIZE_6)) {
            memcpy(p, f, TEXT_CB_SIZE_6);
            r |= UpdatedConstantBuffer;
        }
    }

    QSGD3D12TextureView &tv(pipelineState->shaders.rootSig.textureViews[0]);
    tv.filter = QSGD3D12TextureView::FilterNearest;
    tv.addressModeHoriz = QSGD3D12TextureView::AddressClamp;
    tv.addressModeVert = QSGD3D12TextureView::AddressClamp;

    glyphCache()->useTexture();

    return r;
}

void QSGD3D12TextMaterial::populate(const QPointF &p,
                                    const QVector<quint32> &glyphIndexes,
                                    const QVector<QPointF> &glyphPositions,
                                    QSGGeometry *geometry,
                                    QRectF *boundingRect,
                                    QPointF *baseLine,
                                    const QMargins &margins)
{
    Q_ASSERT(m_font.isValid());
    QVector<QFixedPoint> fixedPointPositions;
    const int glyphPositionsSize = glyphPositions.size();
    fixedPointPositions.reserve(glyphPositionsSize);
    for (int i=0; i < glyphPositionsSize; ++i)
        fixedPointPositions.append(QFixedPoint::fromPointF(glyphPositions.at(i)));

    QSGD3D12GlyphCache *cache = glyphCache();
    QRawFontPrivate *fontD = QRawFontPrivate::get(m_font);
    cache->populate(fontD->fontEngine, glyphIndexes.size(), glyphIndexes.constData(),
                    fixedPointPositions.data());
    cache->fillInPendingGlyphs();

    int margin = fontD->fontEngine->glyphMargin(cache->glyphFormat());

    float glyphCacheScaleX = cache->transform().m11();
    float glyphCacheScaleY = cache->transform().m22();
    float glyphCacheInverseScaleX = 1.0 / glyphCacheScaleX;
    float glyphCacheInverseScaleY = 1.0 / glyphCacheScaleY;

    Q_ASSERT(geometry->indexType() == QSGGeometry::UnsignedShortType);
    geometry->allocate(glyphIndexes.size() * 4, glyphIndexes.size() * 6);
    QVector4D *vp = reinterpret_cast<QVector4D *>(geometry->vertexDataAsTexturedPoint2D());
    Q_ASSERT(geometry->sizeOfVertex() == sizeof(QVector4D));
    ushort *ip = geometry->indexDataAsUShort();

    QPointF position(p.x(), p.y() - m_font.ascent());
    bool supportsSubPixelPositions = fontD->fontEngine->supportsSubPixelPositions();
    for (int i = 0; i < glyphIndexes.size(); ++i) {
         QFixed subPixelPosition;
         if (supportsSubPixelPositions)
             subPixelPosition = fontD->fontEngine->subPixelPositionForX(QFixed::fromReal(glyphPositions.at(i).x()));

         QTextureGlyphCache::GlyphAndSubPixelPosition glyph(glyphIndexes.at(i), subPixelPosition);
         const QTextureGlyphCache::Coord &c = cache->coords.value(glyph);

         QPointF glyphPosition = glyphPositions.at(i) + position;
         float x = (qFloor(glyphPosition.x() * glyphCacheScaleX) * glyphCacheInverseScaleX)
                 + (c.baseLineX * glyphCacheInverseScaleX) - margin;
         float y = (qRound(glyphPosition.y() * glyphCacheScaleY) * glyphCacheInverseScaleY)
                 - (c.baseLineY * glyphCacheInverseScaleY) - margin;

         float w = c.w * glyphCacheInverseScaleX;
         float h = c.h * glyphCacheInverseScaleY;

         *boundingRect |= QRectF(x + margin, y + margin, w, h);

         float cx1 = x - margins.left();
         float cx2 = x + w + margins.right();
         float cy1 = y - margins.top();
         float cy2 = y + h + margins.bottom();

         float tx1 = c.x - margins.left();
         float tx2 = c.x + c.w + margins.right();
         float ty1 = c.y - margins.top();
         float ty2 = c.y + c.h + margins.bottom();

         if (baseLine->isNull())
             *baseLine = glyphPosition;

         vp[4 * i + 0] = QVector4D(cx1, cy1, tx1, ty1);
         vp[4 * i + 1] = QVector4D(cx2, cy1, tx2, ty1);
         vp[4 * i + 2] = QVector4D(cx1, cy2, tx1, ty2);
         vp[4 * i + 3] = QVector4D(cx2, cy2, tx2, ty2);

         int o = i * 4;
         ip[6 * i + 0] = o + 0;
         ip[6 * i + 1] = o + 2;
         ip[6 * i + 2] = o + 3;
         ip[6 * i + 3] = o + 3;
         ip[6 * i + 4] = o + 1;
         ip[6 * i + 5] = o + 0;
    }
}

QT_END_NAMESPACE
