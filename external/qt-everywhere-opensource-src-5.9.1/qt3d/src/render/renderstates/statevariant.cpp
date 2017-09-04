/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include "statevariant_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

void StateVariant::apply(GraphicsContext *gc) const
{
#if !defined(_MSC_VER) || (_MSC_VER > 1800)
    switch (type) {
    case BlendEquationArgumentsMask:
        data.blendEquationArguments.apply(gc);
        return;
    case BlendStateMask:
        data.blendEquation.apply(gc);
        return;
    case AlphaTestMask:
        data.alphaFunc.apply(gc);
        return;
    case MSAAEnabledStateMask:
        data.msaaEnabled.apply(gc);
        return;
    case DepthTestStateMask:
        data.depthTest.apply(gc);
        return;
    case DepthWriteStateMask:
        data.noDepthMask.apply(gc);
        return;
    case CullFaceStateMask:
        data.cullFace.apply(gc);
        return;
    case FrontFaceStateMask:
        data.frontFace.apply(gc);
        return;
    case DitheringStateMask:
        data.dithering.apply(gc);
        return;
    case ScissorStateMask:
        data.scissorTest.apply(gc);
        return;
    case StencilTestStateMask:
        data.stencilTest.apply(gc);
        return;
    case AlphaCoverageStateMask:
        data.alphaCoverage.apply(gc);
        return;
    case PointSizeMask:
        data.pointSize.apply(gc);
        return;
    case PolygonOffsetStateMask:
        data.polygonOffset.apply(gc);
        return;
    case ColorStateMask:
        data.colorMask.apply(gc);
        return;
    case ClipPlaneMask:
        data.clipPlane.apply(gc);
        return;
    case SeamlessCubemapMask:
        data.seamlessCubemap.apply(gc);
        return;
    case StencilOpMask:
        data.stencilOp.apply(gc);
        return;
    case StencilWriteStateMask:
        data.stencilMask.apply(gc);
        return;
    default:
        Q_UNREACHABLE();
    }
#else
    m_impl->apply(gc);
#endif
}

RenderStateImpl *StateVariant::state()
{
#if !defined(_MSC_VER) || (_MSC_VER > 1800)
    switch (type) {
    case BlendEquationArgumentsMask:
    case BlendStateMask:
    case AlphaTestMask:
    case MSAAEnabledStateMask:
    case DepthTestStateMask:
    case DepthWriteStateMask:
    case CullFaceStateMask:
    case FrontFaceStateMask:
    case DitheringStateMask:
    case ScissorStateMask:
    case StencilTestStateMask:
    case AlphaCoverageStateMask:
    case PointSizeMask:
    case PolygonOffsetStateMask:
    case ColorStateMask:
    case ClipPlaneMask:
    case SeamlessCubemapMask:
    case StencilOpMask:
    case StencilWriteStateMask:
        return &data.blendEquationArguments;
    default:
        Q_UNREACHABLE();
    }
#else
    return m_impl.data();
#endif
}

const RenderStateImpl *StateVariant::constState() const
{
#if !defined(_MSC_VER) || (_MSC_VER > 1800)
    switch (type) {
    case BlendEquationArgumentsMask:
    case BlendStateMask:
    case AlphaTestMask:
    case MSAAEnabledStateMask:
    case DepthTestStateMask:
    case DepthWriteStateMask:
    case CullFaceStateMask:
    case FrontFaceStateMask:
    case DitheringStateMask:
    case ScissorStateMask:
    case StencilTestStateMask:
    case AlphaCoverageStateMask:
    case PointSizeMask:
    case PolygonOffsetStateMask:
    case ColorStateMask:
    case ClipPlaneMask:
    case SeamlessCubemapMask:
    case StencilOpMask:
    case StencilWriteStateMask:
        return &data.blendEquationArguments;
    default:
        Q_UNREACHABLE();
    }
#else
   return m_impl.data();
#endif
}

bool StateVariant::operator ==(const StateVariant &other) const
{
    return (other.type == type && constState()->equalTo(*other.constState()));
}

bool StateVariant::operator !=(const StateVariant &other) const
{
    return !(*this == other);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
