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

#include "qt3dquick3drenderplugin.h"
#include <Qt3DRender/qsceneloader.h>
#include <Qt3DRender/qmesh.h>
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qshaderprogram.h>
#include <Qt3DRender/qviewport.h>
#include <Qt3DRender/qrenderpassfilter.h>
#include <Qt3DRender/qtechniquefilter.h>
#include <Qt3DRender/qrendertargetselector.h>
#include <Qt3DRender/qrendersurfaceselector.h>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qcameraselector.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qfilterkey.h>
#include <Qt3DRender/qlayer.h>
#include <Qt3DRender/qabstractlight.h>
#include <Qt3DRender/qspotlight.h>
#include <Qt3DRender/qdirectionallight.h>
#include <Qt3DRender/qpointlight.h>
#include <Qt3DRender/qgraphicsapifilter.h>
#include <Qt3DRender/qrenderstate.h>
#include <Qt3DRender/qblendequationarguments.h>
#include <Qt3DRender/qblendequation.h>
#include <Qt3DRender/qalphatest.h>
#include <Qt3DRender/qdepthtest.h>
#include <Qt3DRender/qnodepthmask.h>
#include <Qt3DRender/qcullface.h>
#include <Qt3DRender/qfrontface.h>
#include <Qt3DRender/qstenciltest.h>
#include <Qt3DRender/qstenciltestarguments.h>
#include <Qt3DRender/qscissortest.h>
#include <Qt3DRender/qdithering.h>
#include <Qt3DRender/qrendertargetoutput.h>
#include <Qt3DRender/qclearbuffers.h>
#include <Qt3DRender/qalphacoverage.h>
#include <Qt3DRender/qmultisampleantialiasing.h>
#include <Qt3DRender/qpointsize.h>
#include <Qt3DRender/qpolygonoffset.h>
#include <Qt3DRender/qcolormask.h>
#include <Qt3DRender/qshaderdata.h>
#include <Qt3DRender/qnodraw.h>
#include <Qt3DRender/qclipplane.h>
#include <Qt3DRender/qseamlesscubemap.h>
#include <Qt3DRender/qsortpolicy.h>
#include <Qt3DRender/qstenciloperation.h>
#include <Qt3DRender/qstenciloperationarguments.h>
#include <Qt3DRender/qstencilmask.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qgeometry.h>
#include <Qt3DRender/qgeometryrenderer.h>
#include <Qt3DRender/qobjectpicker.h>
#include <Qt3DRender/qpickevent.h>
#include <Qt3DRender/qfrustumculling.h>
#include <Qt3DRender/qdispatchcompute.h>
#include <Qt3DRender/qcomputecommand.h>
#include <Qt3DRender/qcameralens.h>
#include <Qt3DRender/qcamera.h>
#include <Qt3DRender/qrendersettings.h>
#include <Qt3DRender/qpickingsettings.h>
#include <Qt3DRender/qrendercapture.h>
#include <Qt3DQuickRender/private/quick3dlayerfilter_p.h>
#include <Qt3DQuickRender/private/quick3dtechnique_p.h>
#include <Qt3DQuickRender/private/quick3dmaterial_p.h>
#include <Qt3DQuickRender/private/quick3dtechniquefilter_p.h>
#include <Qt3DQuickRender/private/quick3dviewport_p.h>
#include <Qt3DQuickRender/private/quick3drenderpassfilter_p.h>
#include <Qt3DQuickRender/private/quick3drendertargetoutput_p.h>
#include <Qt3DQuickRender/private/quick3deffect_p.h>
#include <Qt3DQuickRender/private/quick3dscene_p.h>
#include <Qt3DQuickRender/private/quick3dtexture_p.h>
#include <Qt3DQuickRender/private/quick3drenderpass_p.h>
#include <Qt3DQuickRender/private/quick3dparameter_p.h>
#include <Qt3DQuickRender/private/quick3dshaderdata_p.h>
#include <Qt3DQuickRender/private/quick3dshaderdataarray_p.h>
#include <Qt3DQuickRender/private/quick3dstateset_p.h>
#include <Qt3DQuickRender/private/quick3drendertargetselector_p.h>
#include <Qt3DQuickRender/private/quick3dgeometry_p.h>
#include <Qt3DQuickRender/private/quick3dbuffer_p.h>
#include <Qt3DQuickRender/private/qt3dquickrender_global_p.h>

#include <QtGui/qwindow.h>

QT_BEGIN_NAMESPACE

QVariantList Quick3DShaderDataArrayToVariantListConverter(Qt3DRender::Render::Quick::Quick3DShaderDataArray *array)
{
    const QVector<Qt3DRender::QShaderData *> arrayValues = array->values();
    QVariantList values;
    values.reserve(arrayValues.size());
    for (Qt3DRender::QShaderData *data : arrayValues)
        values.append(QVariant::fromValue(data));
    return values;
}

void Qt3DQuick3DRenderPlugin::registerTypes(const char *uri)
{
    Qt3DRender::Quick::Quick3DRender_initialize();

    qmlRegisterUncreatableType<QWindow>(uri, 2, 0, "Window", "Only for assigning window surface property");

    // Converters
    QMetaType::registerConverter<Qt3DRender::Render::Quick::Quick3DShaderDataArray*, QVariantList>(Quick3DShaderDataArrayToVariantListConverter);

    // Renderer setttings
    qmlRegisterType<Qt3DRender::QRenderSettings>(uri, 2, 0, "RenderSettings");
    qmlRegisterType<Qt3DRender::QPickingSettings>(uri, 2, 0, "PickingSettings");

    // @uri Qt3D.Render
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QSceneLoader, Qt3DRender::Render::Quick::Quick3DScene>("QSceneLoader", "Qt3D.Render/SceneLoader", uri, 2, 0, "SceneLoader");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QEffect, Qt3DRender::Render::Quick::Quick3DEffect>("QEffect", "Qt3D.Render/Effect", uri, 2, 0, "Effect");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTechnique, Qt3DRender::Render::Quick::Quick3DTechnique>("QTechnique", "Qt3D.Render/Technique", uri, 2, 0, "Technique");
    qmlRegisterType<Qt3DRender::QFilterKey>(uri, 2, 0, "FilterKey");
    qmlRegisterType<Qt3DRender::QGraphicsApiFilter>(uri, 2, 0, "GraphicsApiFilter");
    qmlRegisterUncreatableType<Qt3DRender::QParameter>(uri, 2, 0, "QParameter", "Quick3D should instantiate Quick3DParameter only");
    qmlRegisterType<Qt3DRender::Render::Quick::Quick3DParameter>(uri, 2, 0, "Parameter");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QMaterial, Qt3DRender::Render::Quick::Quick3DMaterial>("QMaterial", "Qt3D.Render/Material", uri, 2, 0, "Material");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QRenderPass, Qt3DRender::Render::Quick::Quick3DRenderPass>("QRenderPass", "Qt3D.Render/RenderPass", uri, 2, 0, "RenderPass");
    qmlRegisterType<Qt3DRender::QShaderProgram>(uri, 2, 0, "ShaderProgram");
    qmlRegisterUncreatableType<Qt3DRender::QShaderData>(uri, 2, 0, "QShaderData", "Quick3D should instantiate Quick3DShaderData only");
    qmlRegisterType<Qt3DRender::Render::Quick::Quick3DShaderDataArray>(uri, 2, 0, "ShaderDataArray");
    qmlRegisterType<Qt3DRender::Render::Quick::Quick3DShaderData>(uri, 2, 0, "ShaderData");

    // Camera
    qmlRegisterType<Qt3DRender::QCamera>(uri, 2, 0, "Camera");
    qmlRegisterType<Qt3DRender::QCameraLens>(uri, 2, 0, "CameraLens");

    // Textures
    qmlRegisterType<Qt3DRender::QTextureWrapMode>(uri, 2, 0, "WrapMode");//, QStringLiteral("QTextureWrapMode cannot be created from QML"));
    qmlRegisterUncreatableType<Qt3DRender::QAbstractTexture>(uri, 2, 0, "Texture", QStringLiteral("Texture should be created from one of the subclasses"));
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTexture1D, Qt3DRender::Render::Quick::Quick3DTextureExtension>("QTexture1D", "Qt3D.Render/Texture1D", uri, 2, 0, "Texture1D");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTexture1DArray, Qt3DRender::Render::Quick::Quick3DTextureExtension>("QTexture1DArray", "Qt3D.Render/Texture1DArray", uri, 2, 0, "Texture1DArray");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTexture2D, Qt3DRender::Render::Quick::Quick3DTextureExtension>("QTexture2D", "Qt3D.Render/Texture2D", uri, 2, 0, "Texture2D");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTexture2DArray, Qt3DRender::Render::Quick::Quick3DTextureExtension>("QTexture2DArray", "Qt3D.Render/Texture2DArray", uri, 2, 0, "Texture2DArray");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTexture3D, Qt3DRender::Render::Quick::Quick3DTextureExtension>("QTexture3D", "Qt3D.Render/Texture3D", uri, 2, 0, "Texture3D");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTextureCubeMap, Qt3DRender::Render::Quick::Quick3DTextureExtension>("QTextureCubeMap", "Qt3D.Render/TextureCubeMap", uri, 2, 0, "TextureCubeMap");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTextureCubeMapArray, Qt3DRender::Render::Quick::Quick3DTextureExtension>("QTextureCubeMapArray", "Qt3D.Render/TextureCubeMapArray", uri, 2, 0, "TextureCubeMapArray");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTexture2DMultisample, Qt3DRender::Render::Quick::Quick3DTextureExtension>("QTexture2DMultisample", "Qt3D.Render/Texture2DMultisample", uri, 2, 0, "Texture2DMultisample");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTexture2DMultisampleArray, Qt3DRender::Render::Quick::Quick3DTextureExtension>("QTexture2DMultisampleArray", "Qt3D.Render/Texture2DMultisampleArray", uri, 2, 0, "Texture2DMultisampleArray");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTextureRectangle, Qt3DRender::Render::Quick::Quick3DTextureExtension>("QTextureRectangle", "Qt3D.Render/TextureRectangle", uri, 2, 0, "TextureRectangle");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTextureBuffer, Qt3DRender::Render::Quick::Quick3DTextureExtension>("QTextureBuffer", "Qt3D.Render/TextureBuffer", uri, 2, 0, "TextureBuffer");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTextureLoader, Qt3DRender::Render::Quick::Quick3DTextureExtension>("QTextureLoader", "Qt3D.Render/TextureLoader", uri, 2, 0, "TextureLoader");
    qmlRegisterUncreatableType<Qt3DRender::QAbstractTextureImage>(uri, 2, 0, "QAbstractTextureImage", QStringLiteral("QAbstractTextureImage is abstract"));
    qmlRegisterType<Qt3DRender::QTextureImage>(uri, 2, 0, "TextureImage");

    // Geometry
    qmlRegisterType<Qt3DRender::QAttribute>(uri, 2, 0, "Attribute");
    qmlRegisterUncreatableType<Qt3DRender::QBuffer>(uri, 2, 0, "BufferBase", QStringLiteral("Use Quick3DBuffer in QML"));
    qmlRegisterType<Qt3DRender::Render::Quick::Quick3DBuffer>(uri, 2, 0, "Buffer");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QGeometry, Qt3DRender::Render::Quick::Quick3DGeometry>("QGeometry", "Qt3D.Render/Geometry", uri, 2, 0, "Geometry");
    qmlRegisterType<Qt3DRender::QGeometryRenderer>(uri, 2, 0, "GeometryRenderer");

    // Mesh
    qmlRegisterType<Qt3DRender::QMesh>(uri, 2, 0, "Mesh");

    // Picking
    qmlRegisterType<Qt3DRender::QObjectPicker>(uri, 2, 0, "ObjectPicker");
    qmlRegisterUncreatableType<Qt3DRender::QPickEvent>(uri, 2, 0, "PickEvent", QStringLiteral("Events cannot be created"));

        // Compute Job
    qmlRegisterType<Qt3DRender::QComputeCommand>(uri, 2, 0, "ComputeCommand");

    // Layers
    qmlRegisterType<Qt3DRender::QLayer>(uri, 2, 0, "Layer");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QLayerFilter, Qt3DRender::Render::Quick::Quick3DLayerFilter>("QLayerFilter", "Qt3D.Render/LayerFilter", uri, 2, 0, "LayerFilter");

    // Lights
    qmlRegisterUncreatableType<Qt3DRender::QAbstractLight>(uri, 2, 0, "Light", QStringLiteral("Light is an abstract base class"));
    qmlRegisterType<Qt3DRender::QPointLight>(uri, 2, 0, "PointLight");
    qmlRegisterType<Qt3DRender::QDirectionalLight>(uri, 2, 0, "DirectionalLight");
    qmlRegisterType<Qt3DRender::QSpotLight>(uri, 2, 0, "SpotLight");

    // FrameGraph
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QCameraSelector, Qt3DCore::Quick::Quick3DNode>("QCameraSelector", "Qt3D.Render/CameraSelector", uri, 2, 0, "CameraSelector");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QRenderPassFilter, Qt3DRender::Render::Quick::Quick3DRenderPassFilter>("QRenderPassFilter", "Qt3D.Render/RenderPassFilter", uri, 2, 0, "RenderPassFilter");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QTechniqueFilter, Qt3DRender::Render::Quick::Quick3DTechniqueFilter>("QTechniqueFilter", "Qt3D.Render/TechniqueFilter", uri, 2, 0, "TechniqueFilter");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QViewport, Qt3DRender::Render::Quick::Quick3DViewport>("QViewport", "Qt3D.Render/Viewport", uri, 2, 0, "Viewport");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QRenderTargetSelector, Qt3DRender::Render::Quick::Quick3DRenderTargetSelector>("QRenderTargetSelector", "Qt3D.Render/RenderTargetSelector", uri, 2, 0, "RenderTargetSelector");
    qmlRegisterType<Qt3DRender::QClearBuffers>(uri, 2, 0, "ClearBuffers");
    qmlRegisterType<Qt3DRender::QFrameGraphNode>(uri, 2, 0, "FrameGraphNode");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QRenderStateSet, Qt3DRender::Render::Quick::Quick3DStateSet>("QRenderStateSet", "Qt3D.Render/RenderStateSet", uri, 2, 0, "RenderStateSet");
    qmlRegisterType<Qt3DRender::QNoDraw>(uri, 2, 0, "NoDraw");
    qmlRegisterType<Qt3DRender::QFrustumCulling>(uri, 2, 0, "FrustumCulling");
    qmlRegisterType<Qt3DRender::QDispatchCompute>(uri, 2, 0, "DispatchCompute");
    qmlRegisterType<Qt3DRender::QRenderCapture>(uri, 2, 1, "RenderCapture");
    qmlRegisterUncreatableType<Qt3DRender::QRenderCaptureReply>(uri, 2, 1, "RenderCaptureReply", QStringLiteral("RenderCaptureReply is only instantiated by RenderCapture"));

    // RenderTarget
    qmlRegisterType<Qt3DRender::QRenderTargetOutput>(uri, 2, 0, "RenderTargetOutput");
    Qt3DRender::Quick::registerExtendedType<Qt3DRender::QRenderTarget, Qt3DRender::Render::Quick::Quick3DRenderTargetOutput>("QRenderTarget", "Qt3D.Render/RenderTarget", uri, 2, 0, "RenderTarget");

    // Render surface selector
    qmlRegisterType<Qt3DRender::QRenderSurfaceSelector>(uri, 2, 0, "RenderSurfaceSelector");

    // Sorting
    qmlRegisterType<Qt3DRender::QSortPolicy>(uri, 2, 0, "SortPolicy");

    // RenderStates
    qmlRegisterUncreatableType<Qt3DRender::QRenderState>(uri, 2, 0, "RenderState", QStringLiteral("QRenderState is a base class"));
    qmlRegisterType<Qt3DRender::QBlendEquationArguments>(uri, 2, 0, "BlendEquationArguments");
    qmlRegisterType<Qt3DRender::QBlendEquation>(uri, 2, 0, "BlendEquation");
    qmlRegisterType<Qt3DRender::QAlphaTest>(uri, 2, 0, "AlphaTest");
    qmlRegisterType<Qt3DRender::QDepthTest>(uri, 2, 0, "DepthTest");
    qmlRegisterType<Qt3DRender::QMultiSampleAntiAliasing>(uri, 2, 0, "MultiSampleAntiAliasing");
    qmlRegisterType<Qt3DRender::QNoDepthMask>(uri, 2, 0, "NoDepthMask");
    qmlRegisterType<Qt3DRender::QCullFace>(uri, 2, 0, "CullFace");
    qmlRegisterType<Qt3DRender::QFrontFace>(uri, 2, 0, "FrontFace");
    qmlRegisterUncreatableType<Qt3DRender::QStencilTestArguments>(uri, 2, 0, "StencilTestArguments", QStringLiteral("QStencilTestArguments cannot be instantiated on its own"));
    qmlRegisterType<Qt3DRender::QStencilTest>(uri, 2, 0, "StencilTest");
    qmlRegisterType<Qt3DRender::QScissorTest>(uri, 2, 0, "ScissorTest");
    qmlRegisterType<Qt3DRender::QDithering>(uri, 2, 0, "Dithering");
    qmlRegisterType<Qt3DRender::QAlphaCoverage>(uri, 2, 0, "AlphaCoverage");
    qmlRegisterType<Qt3DRender::QPointSize>(uri, 2, 0, "PointSize");
    qmlRegisterType<Qt3DRender::QPolygonOffset>(uri, 2, 0, "PolygonOffset");
    qmlRegisterType<Qt3DRender::QColorMask>(uri, 2, 0, "ColorMask");
    qmlRegisterType<Qt3DRender::QClipPlane>(uri, 2, 0, "ClipPlane");
    qmlRegisterUncreatableType<Qt3DRender::QStencilOperationArguments>(uri, 2, 0, "StencilOperationArguments", QStringLiteral("StencilOperationArguments cannot be instanciated on its own"));
    qmlRegisterType<Qt3DRender::QSeamlessCubemap>(uri, 2, 0, "SeamlessCubemap");
    qmlRegisterType<Qt3DRender::QStencilOperation>(uri, 2, 0, "StencilOperation");
    qmlRegisterType<Qt3DRender::QStencilMask>(uri, 2, 0, "StencilMask");
}

QT_END_NAMESPACE


