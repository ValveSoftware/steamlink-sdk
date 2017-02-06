/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "renderstates_p.h"

#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DRender/qrenderstate.h>
#include <Qt3DRender/qcullface.h>

#include <Qt3DRender/private/graphicscontext_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

void RenderStateImpl::updateProperty(const char *name, const QVariant &value)
{
    Q_UNUSED(name);
    Q_UNUSED(value);
}

void BlendEquationArguments::apply(GraphicsContext* gc) const
{
    // Un-indexed BlendEquationArguments -> Use normal GL1.0 functions
    if (std::get<5>(m_values) < 0) {
        if (std::get<4>(m_values)) {
            gc->openGLContext()->functions()->glEnable(GL_BLEND);
            gc->openGLContext()->functions()->glBlendFuncSeparate(std::get<0>(m_values), std::get<1>(m_values), std::get<2>(m_values), std::get<3>(m_values));
        } else {
            gc->openGLContext()->functions()->glDisable(GL_BLEND);
        }
    }
    // BlendEquationArguments for a particular Draw Buffer. Different behaviours for
    //  (1) 3.0-3.3: only enablei/disablei supported.
    //  (2) 4.0+: all operations supported.
    // We just ignore blend func parameter for (1), so no warnings get
    // printed.
    else {
        if (std::get<4>(m_values)) {
            gc->enablei(GL_BLEND, std::get<5>(m_values));
            if (gc->supportsDrawBuffersBlend()) {
                gc->blendFuncSeparatei(std::get<5>(m_values), std::get<0>(m_values), std::get<1>(m_values), std::get<2>(m_values), std::get<3>(m_values));
            }
        } else {
            gc->disablei(GL_BLEND, std::get<5>(m_values));
        }
    }
}

void BlendEquationArguments::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("sourceRgb")) std::get<0>(m_values) = value.toInt();
    else if (name == QByteArrayLiteral("destinationRgb")) std::get<1>(m_values) = value.toInt();
    else if (name == QByteArrayLiteral("sourceAlpha")) std::get<2>(m_values) = value.toInt();
    else if (name == QByteArrayLiteral("destinationAlpha")) std::get<3>(m_values) = value.toInt();
    else if (name == QByteArrayLiteral("enabled")) std::get<4>(m_values) = value.toBool();
    else if (name == QByteArrayLiteral("bufferIndex")) std::get<5>(m_values) = value.toInt();
}

void BlendEquation::apply(GraphicsContext *gc) const
{
    gc->blendEquation(std::get<0>(m_values));
}

void BlendEquation::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("mode")) std::get<0>(m_values) = value.toInt();
}

void AlphaFunc::apply(GraphicsContext* gc) const
{
    gc->alphaTest(std::get<0>(m_values), std::get<1>(m_values));
}

void MSAAEnabled::apply(GraphicsContext *gc) const
{
    gc->setMSAAEnabled(std::get<0>(m_values));
}

void MSAAEnabled::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("enabled"))
        std::get<0>(m_values) = value.toBool();
}

void DepthTest::apply(GraphicsContext *gc) const
{
    gc->depthTest(std::get<0>(m_values));
}

void DepthTest::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("func")) std::get<0>(m_values) = value.toInt();
}

void CullFace::apply(GraphicsContext *gc) const
{
    if (std::get<0>(m_values) == QCullFace::NoCulling) {
        gc->openGLContext()->functions()->glDisable(GL_CULL_FACE);
    } else {
        gc->openGLContext()->functions()->glEnable(GL_CULL_FACE);
        gc->openGLContext()->functions()->glCullFace(std::get<0>(m_values));
    }
}

void CullFace::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("mode")) std::get<0>(m_values) = value.toInt();
}

void FrontFace::apply(GraphicsContext *gc) const
{
    gc->frontFace(std::get<0>(m_values));
}

void FrontFace::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("direction")) std::get<0>(m_values) = value.toInt();
}

void NoDepthMask::apply(GraphicsContext *gc) const
{
    gc->depthMask(std::get<0>(m_values));
}

void NoDepthMask::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("mask")) std::get<0>(m_values) = value.toBool();
}

void Dithering::apply(GraphicsContext *gc) const
{
    gc->openGLContext()->functions()->glEnable(GL_DITHER);
}

void ScissorTest::apply(GraphicsContext *gc) const
{
    gc->openGLContext()->functions()->glEnable(GL_SCISSOR_TEST);
    gc->openGLContext()->functions()->glScissor(std::get<0>(m_values), std::get<1>(m_values), std::get<2>(m_values), std::get<3>(m_values));
}

void ScissorTest::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("left")) std::get<0>(m_values) = value.toInt();
    else if (name == QByteArrayLiteral("bottom")) std::get<1>(m_values) = value.toInt();
    else if (name == QByteArrayLiteral("width")) std::get<2>(m_values) = value.toInt();
    else if (name == QByteArrayLiteral("height")) std::get<3>(m_values) = value.toInt();
}

void StencilTest::apply(GraphicsContext *gc) const
{
    gc->openGLContext()->functions()->glEnable(GL_STENCIL_TEST);
    gc->openGLContext()->functions()->glStencilFuncSeparate(GL_FRONT, std::get<0>(m_values), std::get<1>(m_values), std::get<2>(m_values));
    gc->openGLContext()->functions()->glStencilFuncSeparate(GL_BACK, std::get<3>(m_values), std::get<4>(m_values), std::get<5>(m_values));
}

void AlphaCoverage::apply(GraphicsContext *gc) const
{
    gc->setAlphaCoverageEnabled(std::get<0>(m_values));
}

void AlphaCoverage::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("enabled"))
        std::get<0>(m_values) = value.toBool();
}

void PointSize::apply(GraphicsContext *gc) const
{
    gc->pointSize(std::get<0>(m_values), std::get<1>(m_values));
}

void PointSize::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("specification")) std::get<0>(m_values) = value.toBool();
    else if (name == QByteArrayLiteral("value")) std::get<1>(m_values) = value.toFloat();
}

void PolygonOffset::apply(GraphicsContext *gc) const
{
    gc->openGLContext()->functions()->glEnable(GL_POLYGON_OFFSET_FILL);
    gc->openGLContext()->functions()->glPolygonOffset(std::get<0>(m_values), std::get<1>(m_values));
}

void PolygonOffset::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("factor")) std::get<0>(m_values) = value.toFloat();
    else if (name == QByteArrayLiteral("units")) std::get<1>(m_values) = value.toFloat();
}

void ColorMask::apply(GraphicsContext *gc) const
{
    gc->openGLContext()->functions()->glColorMask(std::get<0>(m_values), std::get<1>(m_values), std::get<2>(m_values), std::get<3>(m_values));
}

void ColorMask::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("red")) std::get<0>(m_values) = value.toBool();
    else if (name == QByteArrayLiteral("green")) std::get<1>(m_values) = value.toBool();
    else if (name == QByteArrayLiteral("blue")) std::get<2>(m_values) = value.toBool();
    else if (name == QByteArrayLiteral("alpha")) std::get<3>(m_values) = value.toBool();
}

void ClipPlane::apply(GraphicsContext *gc) const
{
    gc->enableClipPlane(std::get<0>(m_values));
    gc->setClipPlane(std::get<0>(m_values), std::get<1>(m_values), std::get<2>(m_values));
}

void ClipPlane::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("planeIndex")) std::get<0>(m_values) = value.toInt();
    else if (name == QByteArrayLiteral("normal")) std::get<1>(m_values) = value.value<QVector3D>();
    else if (name == QByteArrayLiteral("distance")) std::get<2>(m_values) = value.toFloat();
}

void SeamlessCubemap::apply(GraphicsContext *gc) const
{
    gc->setSeamlessCubemap(std::get<0>(m_values));
}

void SeamlessCubemap::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("enabled")) std::get<0>(m_values) = value.toBool();
}

void StencilOp::apply(GraphicsContext *gc) const
{
    gc->openGLContext()->functions()->glStencilOpSeparate(GL_FRONT, std::get<0>(m_values), std::get<1>(m_values), std::get<2>(m_values));
    gc->openGLContext()->functions()->glStencilOpSeparate(GL_BACK, std::get<3>(m_values), std::get<4>(m_values), std::get<5>(m_values));
}

void StencilMask::apply(GraphicsContext *gc) const
{
    gc->openGLContext()->functions()->glStencilMaskSeparate(GL_FRONT, std::get<0>(m_values));
    gc->openGLContext()->functions()->glStencilMaskSeparate(GL_BACK, std::get<1>(m_values));
}

void StencilMask::updateProperty(const char *name, const QVariant &value)
{
    if (name == QByteArrayLiteral("frontMask")) std::get<0>(m_values) = value.toInt();
    else if (name == QByteArrayLiteral("backMask")) std::get<1>(m_values) = value.toInt();
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
