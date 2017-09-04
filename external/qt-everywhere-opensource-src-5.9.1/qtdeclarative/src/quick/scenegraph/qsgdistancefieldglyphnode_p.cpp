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

#include "qsgdistancefieldglyphnode_p_p.h"
#include <QtQuick/private/qsgdistancefieldutil_p.h>
#include <QtQuick/private/qsgtexture_p.h>
#include <QtGui/qopenglfunctions.h>
#include <QtGui/qsurface.h>
#include <QtGui/qwindow.h>
#include <qmath.h>

QT_BEGIN_NAMESPACE

class QSGDistanceFieldTextMaterialShader : public QSGMaterialShader
{
public:
    QSGDistanceFieldTextMaterialShader();

    virtual void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect);
    virtual char const *const *attributeNames() const;

protected:
    virtual void initialize();

    void updateAlphaRange(ThresholdFunc thresholdFunc, AntialiasingSpreadFunc spreadFunc);
    void updateColor(const QVector4D &c);
    void updateTextureScale(const QVector2D &ts);

    float m_fontScale;
    float m_matrixScale;

    int m_matrix_id;
    int m_textureScale_id;
    int m_alphaMin_id;
    int m_alphaMax_id;
    int m_color_id;

    QVector2D m_lastTextureScale;
    QVector4D m_lastColor;
    float m_lastAlphaMin;
    float m_lastAlphaMax;
};

char const *const *QSGDistanceFieldTextMaterialShader::attributeNames() const {
    static char const *const attr[] = { "vCoord", "tCoord", 0 };
    return attr;
}

QSGDistanceFieldTextMaterialShader::QSGDistanceFieldTextMaterialShader()
    : QSGMaterialShader(),
      m_fontScale(1.0)
    , m_matrixScale(1.0)
    , m_matrix_id(-1)
    , m_textureScale_id(-1)
    , m_alphaMin_id(-1)
    , m_alphaMax_id(-1)
    , m_color_id(-1)
    , m_lastAlphaMin(-1)
    , m_lastAlphaMax(-1)
{
    setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/qt-project.org/scenegraph/shaders/distancefieldtext.vert"));
    setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qt-project.org/scenegraph/shaders/distancefieldtext.frag"));
}

void QSGDistanceFieldTextMaterialShader::updateAlphaRange(ThresholdFunc thresholdFunc, AntialiasingSpreadFunc spreadFunc)
{
    float combinedScale = m_fontScale * m_matrixScale;
    float base = thresholdFunc(combinedScale);
    float range = spreadFunc(combinedScale);
    float alphaMin = qMax(0.0f, base - range);
    float alphaMax = qMin(base + range, 1.0f);
    if (alphaMin != m_lastAlphaMin) {
        program()->setUniformValue(m_alphaMin_id, GLfloat(alphaMin));
        m_lastAlphaMin = alphaMin;
    }
    if (alphaMax != m_lastAlphaMax) {
        program()->setUniformValue(m_alphaMax_id, GLfloat(alphaMax));
        m_lastAlphaMax = alphaMax;
    }
}

void QSGDistanceFieldTextMaterialShader::updateColor(const QVector4D &c)
{
    if (m_lastColor != c) {
        program()->setUniformValue(m_color_id, c);
        m_lastColor = c;
    }
}

void QSGDistanceFieldTextMaterialShader::updateTextureScale(const QVector2D &ts)
{
    if (m_lastTextureScale != ts) {
        program()->setUniformValue(m_textureScale_id, ts);
        m_lastTextureScale = ts;
    }
}

void QSGDistanceFieldTextMaterialShader::initialize()
{
    QSGMaterialShader::initialize();
    m_matrix_id = program()->uniformLocation("matrix");
    m_textureScale_id = program()->uniformLocation("textureScale");
    m_color_id = program()->uniformLocation("color");
    m_alphaMin_id = program()->uniformLocation("alphaMin");
    m_alphaMax_id = program()->uniformLocation("alphaMax");
}

void QSGDistanceFieldTextMaterialShader::updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    Q_ASSERT(oldEffect == 0 || newEffect->type() == oldEffect->type());
    QSGDistanceFieldTextMaterial *material = static_cast<QSGDistanceFieldTextMaterial *>(newEffect);
    QSGDistanceFieldTextMaterial *oldMaterial = static_cast<QSGDistanceFieldTextMaterial *>(oldEffect);

    bool updated = material->updateTextureSize();

    if (oldMaterial == 0
           || material->color() != oldMaterial->color()
           || state.isOpacityDirty()) {
        QVector4D color = material->color();
        color *= state.opacity();
        updateColor(color);
    }

    bool updateRange = false;
    if (oldMaterial == 0
            || material->fontScale() != oldMaterial->fontScale()) {
        m_fontScale = material->fontScale();
        updateRange = true;
    }
    if (state.isMatrixDirty()) {
        program()->setUniformValue(m_matrix_id, state.combinedMatrix());
        m_matrixScale = qSqrt(qAbs(state.determinant())) * state.devicePixelRatio();
        updateRange = true;
    }
    if (updateRange) {
        updateAlphaRange(material->glyphCache()->manager()->thresholdFunc(),
                         material->glyphCache()->manager()->antialiasingSpreadFunc());
    }

    Q_ASSERT(material->glyphCache());

    if (updated
            || oldMaterial == 0
            || oldMaterial->texture()->textureId != material->texture()->textureId) {
        updateTextureScale(QVector2D(1.0 / material->textureSize().width(),
                                     1.0 / material->textureSize().height()));

        QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
        funcs->glBindTexture(GL_TEXTURE_2D, material->texture()->textureId);

        if (updated) {
            // Set the mag/min filters to be linear. We only need to do this when the texture
            // has been recreated.
            funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
}

QSGDistanceFieldTextMaterial::QSGDistanceFieldTextMaterial()
    : m_glyph_cache(0)
    , m_texture(0)
    , m_fontScale(1.0)
{
   setFlag(Blending | RequiresDeterminant, true);
}

QSGDistanceFieldTextMaterial::~QSGDistanceFieldTextMaterial()
{
}

QSGMaterialType *QSGDistanceFieldTextMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

void QSGDistanceFieldTextMaterial::setColor(const QColor &color)
{
    m_color = QVector4D(color.redF() * color.alphaF(),
                        color.greenF() * color.alphaF(),
                        color.blueF() * color.alphaF(),
                        color.alphaF());
}

QSGMaterialShader *QSGDistanceFieldTextMaterial::createShader() const
{
    return new QSGDistanceFieldTextMaterialShader;
}

bool QSGDistanceFieldTextMaterial::updateTextureSize()
{
    if (!m_texture)
        m_texture = m_glyph_cache->glyphTexture(0); // invalid texture

    if (m_texture->size != m_size) {
        m_size = m_texture->size;
        return true;
    } else {
        return false;
    }
}

int QSGDistanceFieldTextMaterial::compare(const QSGMaterial *o) const
{
    Q_ASSERT(o && type() == o->type());
    const QSGDistanceFieldTextMaterial *other = static_cast<const QSGDistanceFieldTextMaterial *>(o);
    if (m_glyph_cache != other->m_glyph_cache)
        return m_glyph_cache - other->m_glyph_cache;
    if (m_fontScale != other->m_fontScale) {
        return int(other->m_fontScale < m_fontScale) - int(m_fontScale < other->m_fontScale);
    }
    if (m_color != other->m_color)
        return &m_color < &other->m_color ? -1 : 1;
    int t0 = m_texture ? m_texture->textureId : 0;
    int t1 = other->m_texture ? other->m_texture->textureId : 0;
    return t0 - t1;
}


class DistanceFieldStyledTextMaterialShader : public QSGDistanceFieldTextMaterialShader
{
public:
    DistanceFieldStyledTextMaterialShader();

    virtual void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect);

protected:
    virtual void initialize();

    int m_styleColor_id;
};

DistanceFieldStyledTextMaterialShader::DistanceFieldStyledTextMaterialShader()
    : QSGDistanceFieldTextMaterialShader()
    , m_styleColor_id(-1)
{
}

void DistanceFieldStyledTextMaterialShader::initialize()
{
    QSGDistanceFieldTextMaterialShader::initialize();
    m_styleColor_id = program()->uniformLocation("styleColor");
}

void DistanceFieldStyledTextMaterialShader::updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    QSGDistanceFieldTextMaterialShader::updateState(state, newEffect, oldEffect);

    QSGDistanceFieldStyledTextMaterial *material = static_cast<QSGDistanceFieldStyledTextMaterial *>(newEffect);
    QSGDistanceFieldStyledTextMaterial *oldMaterial = static_cast<QSGDistanceFieldStyledTextMaterial *>(oldEffect);

    if (oldMaterial == 0
           || material->styleColor() != oldMaterial->styleColor()
           || (state.isOpacityDirty())) {
        QVector4D color = material->styleColor();
        color *= state.opacity();
        program()->setUniformValue(m_styleColor_id, color);
    }
}

QSGDistanceFieldStyledTextMaterial::QSGDistanceFieldStyledTextMaterial()
    : QSGDistanceFieldTextMaterial()
{
}

QSGDistanceFieldStyledTextMaterial::~QSGDistanceFieldStyledTextMaterial()
{
}

void QSGDistanceFieldStyledTextMaterial::setStyleColor(const QColor &color)
{
    m_styleColor = QVector4D(color.redF() * color.alphaF(),
                             color.greenF() * color.alphaF(),
                             color.blueF() * color.alphaF(),
                             color.alphaF());
}

int QSGDistanceFieldStyledTextMaterial::compare(const QSGMaterial *o) const
{
    Q_ASSERT(o && type() == o->type());
    const QSGDistanceFieldStyledTextMaterial *other = static_cast<const QSGDistanceFieldStyledTextMaterial *>(o);
    if (m_styleColor != other->m_styleColor)
        return &m_styleColor < &other->m_styleColor ? -1 : 1;
    return QSGDistanceFieldTextMaterial::compare(o);
}


class DistanceFieldOutlineTextMaterialShader : public DistanceFieldStyledTextMaterialShader
{
public:
    DistanceFieldOutlineTextMaterialShader();

    virtual void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect);

protected:
    virtual void initialize();

    void updateOutlineAlphaRange(ThresholdFunc thresholdFunc, AntialiasingSpreadFunc spreadFunc, int dfRadius);

    int m_outlineAlphaMax0_id;
    int m_outlineAlphaMax1_id;
};

DistanceFieldOutlineTextMaterialShader::DistanceFieldOutlineTextMaterialShader()
    : DistanceFieldStyledTextMaterialShader()
    , m_outlineAlphaMax0_id(-1)
    , m_outlineAlphaMax1_id(-1)
{
    setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qt-project.org/scenegraph/shaders/distancefieldoutlinetext.frag"));
}

void DistanceFieldOutlineTextMaterialShader::initialize()
{
    DistanceFieldStyledTextMaterialShader::initialize();
    m_outlineAlphaMax0_id = program()->uniformLocation("outlineAlphaMax0");
    m_outlineAlphaMax1_id = program()->uniformLocation("outlineAlphaMax1");
}

void DistanceFieldOutlineTextMaterialShader::updateOutlineAlphaRange(ThresholdFunc thresholdFunc,
                                                                     AntialiasingSpreadFunc spreadFunc,
                                                                     int dfRadius)
{
    float combinedScale = m_fontScale * m_matrixScale;
    float base = thresholdFunc(combinedScale);
    float range = spreadFunc(combinedScale);
    float outlineLimit = qMax(0.2f, base - 0.5f / dfRadius / m_fontScale);

    float alphaMin = qMax(0.0f, base - range);
    float styleAlphaMin0 = qMax(0.0f, outlineLimit - range);
    float styleAlphaMin1 = qMin(outlineLimit + range, alphaMin);
    program()->setUniformValue(m_outlineAlphaMax0_id, GLfloat(styleAlphaMin0));
    program()->setUniformValue(m_outlineAlphaMax1_id, GLfloat(styleAlphaMin1));
}

void DistanceFieldOutlineTextMaterialShader::updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    DistanceFieldStyledTextMaterialShader::updateState(state, newEffect, oldEffect);

    QSGDistanceFieldOutlineTextMaterial *material = static_cast<QSGDistanceFieldOutlineTextMaterial *>(newEffect);
    QSGDistanceFieldOutlineTextMaterial *oldMaterial = static_cast<QSGDistanceFieldOutlineTextMaterial *>(oldEffect);

    if (oldMaterial == 0
            || material->fontScale() != oldMaterial->fontScale()
            || state.isMatrixDirty())
        updateOutlineAlphaRange(material->glyphCache()->manager()->thresholdFunc(),
                                material->glyphCache()->manager()->antialiasingSpreadFunc(),
                                material->glyphCache()->distanceFieldRadius());
}


QSGDistanceFieldOutlineTextMaterial::QSGDistanceFieldOutlineTextMaterial()
    : QSGDistanceFieldStyledTextMaterial()
{
}

QSGDistanceFieldOutlineTextMaterial::~QSGDistanceFieldOutlineTextMaterial()
{
}

QSGMaterialType *QSGDistanceFieldOutlineTextMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *QSGDistanceFieldOutlineTextMaterial::createShader() const
{
    return new DistanceFieldOutlineTextMaterialShader;
}


class DistanceFieldShiftedStyleTextMaterialShader : public DistanceFieldStyledTextMaterialShader
{
public:
    DistanceFieldShiftedStyleTextMaterialShader();

    virtual void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect);

protected:
    virtual void initialize();

    void updateShift(qreal fontScale, const QPointF& shift);

    int m_shift_id;
};

DistanceFieldShiftedStyleTextMaterialShader::DistanceFieldShiftedStyleTextMaterialShader()
    : DistanceFieldStyledTextMaterialShader()
    , m_shift_id(-1)
{
    setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/qt-project.org/scenegraph/shaders/distancefieldshiftedtext.vert"));
    setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qt-project.org/scenegraph/shaders/distancefieldshiftedtext.frag"));
}

void DistanceFieldShiftedStyleTextMaterialShader::initialize()
{
    DistanceFieldStyledTextMaterialShader::initialize();
    m_shift_id = program()->uniformLocation("shift");
}

void DistanceFieldShiftedStyleTextMaterialShader::updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    DistanceFieldStyledTextMaterialShader::updateState(state, newEffect, oldEffect);

    QSGDistanceFieldShiftedStyleTextMaterial *material = static_cast<QSGDistanceFieldShiftedStyleTextMaterial *>(newEffect);
    QSGDistanceFieldShiftedStyleTextMaterial *oldMaterial = static_cast<QSGDistanceFieldShiftedStyleTextMaterial *>(oldEffect);

    if (oldMaterial == 0
            || oldMaterial->fontScale() != material->fontScale()
            || oldMaterial->shift() != material->shift()
            || oldMaterial->textureSize() != material->textureSize()) {
        updateShift(material->fontScale(), material->shift());
    }
}

void DistanceFieldShiftedStyleTextMaterialShader::updateShift(qreal fontScale, const QPointF &shift)
{
    QPointF texel(1.0 / fontScale * shift.x(),
                  1.0 / fontScale * shift.y());
    program()->setUniformValue(m_shift_id, texel);
}

QSGDistanceFieldShiftedStyleTextMaterial::QSGDistanceFieldShiftedStyleTextMaterial()
    : QSGDistanceFieldStyledTextMaterial()
{
}

QSGDistanceFieldShiftedStyleTextMaterial::~QSGDistanceFieldShiftedStyleTextMaterial()
{
}

QSGMaterialType *QSGDistanceFieldShiftedStyleTextMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *QSGDistanceFieldShiftedStyleTextMaterial::createShader() const
{
    return new DistanceFieldShiftedStyleTextMaterialShader;
}

int QSGDistanceFieldShiftedStyleTextMaterial::compare(const QSGMaterial *o) const
{
    const QSGDistanceFieldShiftedStyleTextMaterial *other = static_cast<const QSGDistanceFieldShiftedStyleTextMaterial *>(o);
    if (m_shift != other->m_shift)
        return &m_shift < &other->m_shift ? -1 : 1;
    return QSGDistanceFieldStyledTextMaterial::compare(o);
}

class QSGHiQSubPixelDistanceFieldTextMaterialShader : public QSGDistanceFieldTextMaterialShader
{
public:
    QSGHiQSubPixelDistanceFieldTextMaterialShader();

    virtual void initialize();
    virtual void activate();
    virtual void deactivate();
    virtual void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect);

private:
    int m_fontScale_id;
    int m_vecDelta_id;
};

QSGHiQSubPixelDistanceFieldTextMaterialShader::QSGHiQSubPixelDistanceFieldTextMaterialShader()
    : QSGDistanceFieldTextMaterialShader()
    , m_fontScale_id(-1)
    , m_vecDelta_id(-1)
{
    setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/qt-project.org/scenegraph/shaders/hiqsubpixeldistancefieldtext.vert"));
    setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qt-project.org/scenegraph/shaders/hiqsubpixeldistancefieldtext.frag"));
}

void QSGHiQSubPixelDistanceFieldTextMaterialShader::initialize()
{
    QSGDistanceFieldTextMaterialShader::initialize();
    m_fontScale_id = program()->uniformLocation("fontScale");
    m_vecDelta_id = program()->uniformLocation("vecDelta");
}

void QSGHiQSubPixelDistanceFieldTextMaterialShader::activate()
{
    QSGDistanceFieldTextMaterialShader::activate();
    QOpenGLContext::currentContext()->functions()->glBlendFunc(GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_COLOR);
}

void QSGHiQSubPixelDistanceFieldTextMaterialShader::deactivate()
{
    QSGDistanceFieldTextMaterialShader::deactivate();
    QOpenGLContext::currentContext()->functions()->glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void QSGHiQSubPixelDistanceFieldTextMaterialShader::updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    Q_ASSERT(oldEffect == 0 || newEffect->type() == oldEffect->type());
    QSGDistanceFieldTextMaterial *material = static_cast<QSGDistanceFieldTextMaterial *>(newEffect);
    QSGDistanceFieldTextMaterial *oldMaterial = static_cast<QSGDistanceFieldTextMaterial *>(oldEffect);

    if (oldMaterial == 0 || material->color() != oldMaterial->color()) {
        QVector4D c = material->color();
        state.context()->functions()->glBlendColor(c.x(), c.y(), c.z(), 1.0f);
    }

    if (oldMaterial == 0 || material->fontScale() != oldMaterial->fontScale())
        program()->setUniformValue(m_fontScale_id, GLfloat(material->fontScale()));

    if (oldMaterial == 0 || state.isMatrixDirty()) {
        int viewportWidth = state.viewportRect().width();
        QMatrix4x4 mat = state.combinedMatrix().inverted();
        program()->setUniformValue(m_vecDelta_id, mat.column(0) * (qreal(2) / viewportWidth));
    }

    QSGDistanceFieldTextMaterialShader::updateState(state, newEffect, oldEffect);
}

QSGMaterialType *QSGHiQSubPixelDistanceFieldTextMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *QSGHiQSubPixelDistanceFieldTextMaterial::createShader() const
{
    return new QSGHiQSubPixelDistanceFieldTextMaterialShader;
}


class QSGLoQSubPixelDistanceFieldTextMaterialShader : public QSGHiQSubPixelDistanceFieldTextMaterialShader
{
public:
    QSGLoQSubPixelDistanceFieldTextMaterialShader();
};

QSGLoQSubPixelDistanceFieldTextMaterialShader::QSGLoQSubPixelDistanceFieldTextMaterialShader()
    : QSGHiQSubPixelDistanceFieldTextMaterialShader()
{
    setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/qt-project.org/scenegraph/shaders/loqsubpixeldistancefieldtext.vert"));
    setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qt-project.org/scenegraph/shaders/loqsubpixeldistancefieldtext.frag"));
}

QSGMaterialType *QSGLoQSubPixelDistanceFieldTextMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *QSGLoQSubPixelDistanceFieldTextMaterial::createShader() const
{
    return new QSGLoQSubPixelDistanceFieldTextMaterialShader;
}

QT_END_NAMESPACE
