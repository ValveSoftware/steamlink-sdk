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

#ifndef QT3DRENDER_RENDER_SHADERPARAMETERPACK_P_H
#define QT3DRENDER_RENDER_SHADERPARAMETERPACK_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QVariant>
#include <QByteArray>
#include <QVector>
#include <QOpenGLShaderProgram>
#include <Qt3DCore/qnodeid.h>
#include <Qt3DRender/private/renderlogging_p.h>
#include <Qt3DRender/private/shadervariables_p.h>
#include <Qt3DRender/private/uniform_p.h>

QT_BEGIN_NAMESPACE

class QOpenGLShaderProgram;

namespace Qt3DCore {
class QFrameAllocator;
}

namespace Qt3DRender {
namespace Render {

class GraphicsContext;

struct BlockToUBO {
    int m_blockIndex;
    Qt3DCore::QNodeId m_bufferID;
    bool m_needsUpdate;
    QHash<QString, QVariant> m_updatedProperties;
};
QT3D_DECLARE_TYPEINFO_2(Qt3DRender, Render, BlockToUBO, Q_MOVABLE_TYPE)

struct BlockToSSBO {
    int m_blockIndex;
    Qt3DCore::QNodeId m_bufferID;
};
QT3D_DECLARE_TYPEINFO_2(Qt3DRender, Render, BlockToSSBO, Q_PRIMITIVE_TYPE)


typedef QHash<int, UniformValue> PackUniformHash;

class ShaderParameterPack
{
public:
    ~ShaderParameterPack();

    void setUniform(const int glslNameId, const UniformValue &val);
    void setTexture(const int glslNameId, Qt3DCore::QNodeId id);
    void setUniformBuffer(BlockToUBO blockToUBO);
    void setShaderStorageBuffer(BlockToSSBO blockToSSBO);
    void setSubmissionUniform(const ShaderUniform &uniform);

    inline PackUniformHash &uniforms() { return m_uniforms; }
    inline const PackUniformHash &uniforms() const { return m_uniforms; }
    UniformValue uniform(const int glslNameId) const { return m_uniforms.value(glslNameId); }

    struct NamedTexture
    {
        NamedTexture() {}
        NamedTexture(const int nm, Qt3DCore::QNodeId t)
            : glslNameId(nm)
            , texId(t)
        { }

        int glslNameId;
        Qt3DCore::QNodeId texId;
    };

    inline QVector<NamedTexture> textures() const { return m_textures; }
    inline QVector<BlockToUBO> uniformBuffers() const { return m_uniformBuffers; }
    inline QVector<BlockToSSBO> shaderStorageBuffers() const { return m_shaderStorageBuffers; }
    inline QVector<ShaderUniform> submissionUniforms() const { return m_submissionUniforms; }
private:
    PackUniformHash m_uniforms;

    QVector<NamedTexture> m_textures;
    QVector<BlockToUBO> m_uniformBuffers;
    QVector<BlockToSSBO> m_shaderStorageBuffers;
    QVector<ShaderUniform> m_submissionUniforms;

    friend class RenderView;
};
QT3D_DECLARE_TYPEINFO_2(Qt3DRender, Render, ShaderParameterPack::NamedTexture, Q_PRIMITIVE_TYPE)

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_RENDER_SHADERPARAMETERPACK_P_H
