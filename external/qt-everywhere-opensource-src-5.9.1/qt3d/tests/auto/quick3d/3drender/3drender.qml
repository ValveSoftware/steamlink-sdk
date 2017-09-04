/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/


import Qt3D.Render 2.0 as QQ3Render20
import Qt3D.Render 2.1 as QQ3Render21
import QtQuick 2.0

Item {

    // Misc
    //QQ3Render20.Window                       // (uncreatable) QWindow

    // Converters

    // Renderer setttings
    QQ3Render20.RenderSettings {}              //Qt3DRender::QRenderSettings
    QQ3Render20.PickingSettings {}             //Qt3DRender::QPickingSettings

    // @uri Qt3D.Render
    QQ3Render20.SceneLoader {}                 //Qt3DRender::QSceneLoader, Qt3DRender::Render::Quick::Quick3DScene
    QQ3Render20.Effect {}                      //Qt3DRender::QEffect, Qt3DRender::Render::Quick::Quick3DEffect
    QQ3Render20.Technique {}                   //Qt3DRender::QTechnique, Qt3DRender::Render::Quick::Quick3DTechnique
    QQ3Render20.FilterKey {}                   //Qt3DRender::QFilterKey
    QQ3Render20.GraphicsApiFilter {}           //Qt3DRender::QGraphicsApiFilter
    //QQ3Render20.QParameter                   // (uncreatable) Qt3DRender::QParameter
    QQ3Render20.Parameter {}                   //Qt3DRender::Render::Quick::Quick3DParameter
    QQ3Render20.Material {}                    //Qt3DRender::QMaterial, Qt3DRender::Render::Quick::Quick3DMaterial
    QQ3Render20.RenderPass {}                  //Qt3DRender::QRenderPass, Qt3DRender::Render::Quick::Quick3DRenderPass
    QQ3Render20.ShaderProgram {}               //Qt3DRender::QShaderProgram
    //QQ3Render20.QShaderData                  // (uncreatable) Qt3DRender::QShaderData
    QQ3Render20.ShaderDataArray {}             //Qt3DRender::Render::Quick::Quick3DShaderDataArray
    QQ3Render20.ShaderData {}                  //Qt3DRender::Render::Quick::Quick3DShaderData

    // Camera
    QQ3Render20.Camera {}                      //Qt3DRender::QCamera
    QQ3Render20.CameraLens {}                  //Qt3DRender::QCameraLens

    // Textures
    QQ3Render20.WrapMode {}                    //Qt3DRender::QTextureWrapMode
    //QQ3Render20.Texture                      // (uncreatable) Qt3DRender::QAbstractTexture
    QQ3Render20.Texture1D {}                   //Qt3DRender::QTexture1D, Qt3DRender::Render::Quick::Quick3DTextureExtension
    QQ3Render20.Texture1DArray {}              //Qt3DRender::QTexture1DArray, Qt3DRender::Render::Quick::Quick3DTextureExtension
    QQ3Render20.Texture2D {}                   //Qt3DRender::QTexture2D, Qt3DRender::Render::Quick::Quick3DTextureExtension
    QQ3Render20.Texture2DArray {}              //Qt3DRender::QTexture2DArray, Qt3DRender::Render::Quick::Quick3DTextureExtension
    QQ3Render20.Texture3D {}                   //Qt3DRender::QTexture3D, Qt3DRender::Render::Quick::Quick3DTextureExtension
    QQ3Render20.TextureCubeMap {}              //Qt3DRender::QTextureCubeMap, Qt3DRender::Render::Quick::Quick3DTextureExtension
    QQ3Render20.TextureCubeMapArray {}         //Qt3DRender::QTextureCubeMapArray, Qt3DRender::Render::Quick::Quick3DTextureExtension
    QQ3Render20.Texture2DMultisample {}        //Qt3DRender::QTexture2DMultisample, Qt3DRender::Render::Quick::Quick3DTextureExtension
    QQ3Render20.Texture2DMultisampleArray {}   //Qt3DRender::QTexture2DMultisampleArray, Qt3DRender::Render::Quick::Quick3DTextureExtension
    QQ3Render20.TextureRectangle {}            //Qt3DRender::QTextureRectangle, Qt3DRender::Render::Quick::Quick3DTextureExtension
    QQ3Render20.TextureBuffer {}               //Qt3DRender::QTextureBuffer, Qt3DRender::Render::Quick::Quick3DTextureExtension
    QQ3Render20.TextureLoader {}               //Qt3DRender::QTextureLoader, Qt3DRender::Render::Quick::Quick3DTextureExtension
    //QQ3Render20.QAbstractTextureImage        // (uncreatable) Qt3DRender::QAbstractTextureImage
    QQ3Render20.TextureImage {}                //Qt3DRender::QTextureImage

    // Geometry
    QQ3Render20.Attribute {}                   //Qt3DRender::QAttribute
    //QQ3Render20.BufferBase                   // (uncreatable) Qt3DRender::QBuffer
    QQ3Render20.Buffer {}                      //Qt3DRender::Render::Quick::Quick3DBuffer
    QQ3Render20.Geometry {}                    //Qt3DRender::QGeometry, Qt3DRender::Render::Quick::Quick3DGeometry
    QQ3Render20.GeometryRenderer {}            //Qt3DRender::QGeometryRenderer

    // Mesh
    QQ3Render20.Mesh {}                        //Qt3DRender::QMesh

    // Picking
    QQ3Render20.ObjectPicker {}                //Qt3DRender::QObjectPicker
    //QQ3Render20.PickEvent                    // (uncreatable) Qt3DRender::QPickEvent

    // Compute Job
    QQ3Render20.ComputeCommand {}              //Qt3DRender::QComputeCommand

    // Layers
    QQ3Render20.Layer {}                       //Qt3DRender::QLayer
    QQ3Render20.LayerFilter {}                 //Qt3DRender::QLayerFilter, Qt3DRender::Render::Quick::Quick3DLayerFilter

    // Lights
    //QQ3Render20.Light                        // (uncreatable) Qt3DRender::QAbstractLight
    QQ3Render20.PointLight {}                  //Qt3DRender::QPointLight
    QQ3Render20.DirectionalLight {}            //Qt3DRender::QDirectionalLight
    QQ3Render20.SpotLight {}                   //Qt3DRender::QSpotLight

    // FrameGraph
    QQ3Render20.CameraSelector {}              //Qt3DRender::QCameraSelector, Qt3DCore::Quick::Quick3DNode
    QQ3Render20.RenderPassFilter {}            //Qt3DRender::QRenderPassFilter, Qt3DRender::Render::Quick::Quick3DRenderPassFilter
    QQ3Render20.TechniqueFilter {}             //Qt3DRender::QTechniqueFilter, Qt3DRender::Render::Quick::Quick3DTechniqueFilter
    QQ3Render20.Viewport {}                    //Qt3DRender::QViewport, Qt3DRender::Render::Quick::Quick3DViewport
    QQ3Render20.RenderTargetSelector {}        //Qt3DRender::QRenderTargetSelector, Qt3DRender::Render::Quick::Quick3DRenderTargetSelector
    QQ3Render20.ClearBuffers {}                //Qt3DRender::QClearBuffers
    //QQ3Render20.FrameGraphNode               // (uncreatable) Qt3DRender::QFrameGraphNode
    QQ3Render20.RenderStateSet {}              //Qt3DRender::QRenderStateSet, Qt3DRender::Render::Quick::Quick3DStateSet
    QQ3Render20.NoDraw {}                      //Qt3DRender::QNoDraw
    QQ3Render20.FrustumCulling {}              //Qt3DRender::QFrustumCulling
    QQ3Render20.DispatchCompute {}             //Qt3DRender::QDispatchCompute
    QQ3Render21.RenderCapture {}               //Qt3DRender::QRenderCapture
    //QQ3Render21.RenderCaptureReply           // (uncreatable) Qt3DRender::QRenderCaptureReply

    // RenderTarget
    QQ3Render20.RenderTargetOutput {}          //Qt3DRender::QRenderTargetOutput
    QQ3Render20.RenderTarget {}                //Qt3DRender::QRenderTarget, Qt3DRender::Render::Quick::Quick3DRenderTargetOutput

    // Render surface selector
    QQ3Render20.RenderSurfaceSelector {}       //Qt3DRender::QRenderSurfaceSelector

    // Sorting
    QQ3Render20.SortPolicy {}                  //Qt3DRender::QSortPolicy

    // RenderStates
    //QQ3Render20.RenderState                  // (uncreatable) Qt3DRender::QRenderState
    QQ3Render20.BlendEquationArguments {}      //Qt3DRender::QBlendEquationArguments
    QQ3Render20.BlendEquation {}               //Qt3DRender::QBlendEquation
    QQ3Render20.AlphaTest {}                   //Qt3DRender::QAlphaTest
    QQ3Render20.DepthTest {}                   //Qt3DRender::QDepthTest
    QQ3Render20.MultiSampleAntiAliasing {}     //Qt3DRender::QMultiSampleAntiAliasing
    QQ3Render20.NoDepthMask {}                 //Qt3DRender::QNoDepthMask
    QQ3Render20.CullFace {}                    //Qt3DRender::QCullFace
    QQ3Render20.FrontFace {}                   //Qt3DRender::QFrontFace
    //QQ3Render20.StencilTestArguments         // (uncreatable) Qt3DRender::QStencilTestArguments
    QQ3Render20.StencilTest {}                 //Qt3DRender::QStencilTest
    QQ3Render20.ScissorTest {}                 //Qt3DRender::QScissorTest
    QQ3Render20.Dithering {}                   //Qt3DRender::QDithering
    QQ3Render20.AlphaCoverage {}               //Qt3DRender::QAlphaCoverage
    QQ3Render20.PointSize {}                   //Qt3DRender::QPointSize
    QQ3Render20.PolygonOffset {}               //Qt3DRender::QPolygonOffset
    QQ3Render20.ColorMask {}                   //Qt3DRender::QColorMask
    QQ3Render20.ClipPlane {}                   //Qt3DRender::QClipPlane
    //QQ3Render20.StencilOperationArguments    // (uncreatable) Qt3DRender::QStencilOperationArguments
    QQ3Render20.SeamlessCubemap {}             //Qt3DRender::QSeamlessCubemap
    QQ3Render20.StencilOperation {}            //Qt3DRender::QStencilOperation
    QQ3Render20.StencilMask {}                 //Qt3DRender::QStencilMask

}
