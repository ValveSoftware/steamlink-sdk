/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include "shaderparameterpack_p.h"

#include <Qt3DRender/private/graphicscontext_p.h>
#include <Qt3DRender/private/texture_p.h>

#include <Qt3DCore/private/qframeallocator_p.h>

#include <QOpenGLShaderProgram>
#include <QDebug>
#include <QColor>
#include <QQuaternion>
#include <Qt3DRender/private/renderlogging_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

ShaderParameterPack::~ShaderParameterPack()
{
    m_uniforms.clear();
}

void ShaderParameterPack::setUniform(const int glslNameId, const UniformValue &val)
{
    m_uniforms.insert(glslNameId, val);
}

void ShaderParameterPack::setTexture(const int glslNameId, Qt3DCore::QNodeId texId)
{
    for (int t=0; t<m_textures.size(); ++t) {
        if (m_textures[t].glslNameId != glslNameId)
            continue;

        m_textures[t].texId = texId;
        return;
    }

    m_textures.append(NamedTexture(glslNameId, texId));
}

// Contains Uniform Block Index and QNodeId of the ShaderData (UBO)
void ShaderParameterPack::setUniformBuffer(BlockToUBO blockToUBO)
{
    m_uniformBuffers.append(std::move(blockToUBO));
}

void ShaderParameterPack::setShaderStorageBuffer(BlockToSSBO blockToSSBO)
{
    m_shaderStorageBuffers.push_back(std::move(blockToSSBO));
}

void ShaderParameterPack::setSubmissionUniform(const ShaderUniform &uniform)
{
    m_submissionUniforms.push_back(uniform);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
