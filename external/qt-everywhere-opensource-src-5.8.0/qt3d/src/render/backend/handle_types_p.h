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

#ifndef QT3DRENDER_RENDER_HANDLE_TYPES_P_H
#define QT3DRENDER_RENDER_HANDLE_TYPES_P_H

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

#include <Qt3DRender/qt3drender_global.h>
#include <Qt3DCore/private/qhandle_p.h>

QT_BEGIN_NAMESPACE

class QMatrix4x4;

namespace Qt3DRender {

class QTextureImageData;

namespace Render {

class RenderTargetOutput;
class CameraLens;
class FilterKey;
class Effect;
class Entity;
class Shader;
class FrameGraphNode;
class Layer;
class Material;
class Technique;
class Texture;
class Transform;
class RenderTarget;
class RenderPass;
class Parameter;
class ShaderData;
class TextureImage;
class Buffer;
class Attribute;
class Geometry;
class GeometryRenderer;
class ObjectPicker;
class BoundingVolumeDebug;
class OpenGLVertexArrayObject;
class Light;
class ComputeCommand;
class GLBuffer;
class RenderStateNode;

typedef Qt3DCore::QHandle<RenderTargetOutput, 16> HAttachment;
typedef Qt3DCore::QHandle<CameraLens, 8> HCamera;
typedef Qt3DCore::QHandle<FilterKey, 16> HFilterKey;
typedef Qt3DCore::QHandle<Effect, 16> HEffect;
typedef Qt3DCore::QHandle<Entity, 16> HEntity;
typedef Qt3DCore::QHandle<FrameGraphNode *, 8> HFrameGraphNode;
typedef Qt3DCore::QHandle<Layer, 16> HLayer;
typedef Qt3DCore::QHandle<Material, 16> HMaterial;
typedef Qt3DCore::QHandle<QMatrix4x4, 16> HMatrix;
typedef Qt3DCore::QHandle<OpenGLVertexArrayObject, 16> HVao;
typedef Qt3DCore::QHandle<Shader, 16> HShader;
typedef Qt3DCore::QHandle<Technique, 16> HTechnique;
typedef Qt3DCore::QHandle<Texture, 16> HTexture;
typedef Qt3DCore::QHandle<Transform, 16> HTransform;
typedef Qt3DCore::QHandle<RenderTarget, 8> HTarget;
typedef Qt3DCore::QHandle<RenderPass, 16> HRenderPass;
typedef Qt3DCore::QHandle<QTextureImageData, 16> HTextureData;
typedef Qt3DCore::QHandle<Parameter, 16> HParameter;
typedef Qt3DCore::QHandle<ShaderData, 16> HShaderData;
typedef Qt3DCore::QHandle<TextureImage, 16> HTextureImage;
typedef Qt3DCore::QHandle<Buffer, 16> HBuffer;
typedef Qt3DCore::QHandle<Attribute, 20> HAttribute;
typedef Qt3DCore::QHandle<Geometry, 16> HGeometry;
typedef Qt3DCore::QHandle<GeometryRenderer, 16> HGeometryRenderer;
typedef Qt3DCore::QHandle<ObjectPicker, 16> HObjectPicker;
typedef Qt3DCore::QHandle<BoundingVolumeDebug, 16> HBoundingVolumeDebug;
typedef Qt3DCore::QHandle<Light, 16> HLight;
typedef Qt3DCore::QHandle<ComputeCommand, 16> HComputeCommand;
typedef Qt3DCore::QHandle<GLBuffer, 16> HGLBuffer;
typedef Qt3DCore::QHandle<RenderStateNode, 16> HRenderState;

} // namespace Render

} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_RENDER_HANDLE_TYPES_P_H
