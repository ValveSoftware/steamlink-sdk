/****************************************************************************
**
** Copyright (C) 2008-2012 NVIDIA Corporation.
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Quick 3D.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSSG_RENDER_SHADER_GENERATOR_IMPL_H
#define QSSG_RENDER_SHADER_GENERATOR_IMPL_H

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

#include <QtQuick3DRuntimeRender/private/qssgrenderer_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrenderableobjects_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrendererimplshaders_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrendererimpllayerrenderdata_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrendermesh_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrendermodel_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrenderdefaultmaterial_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrenderlayer_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrenderray_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrendercamera_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrendershadercache_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrendercontextcore_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrendererimpllayerrenderhelper_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrendershadercodegenerator_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrenderclippingfrustum_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrendershaderkeys_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrendershadercache_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrendergpuprofiler_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrenderdefaultmaterialshadergenerator_p.h>
#include <QtQuick3DRuntimeRender/private/qssgrenderbuffermanager_p.h>
#include <QtQuick3DRender/private/qssgrendercontext_p.h>
#include <QtQuick3DRender/private/qssgrendershaderprogram_p.h>

#include <QtQuick3DUtils/private/qssgbounds3_p.h>
#include <QtQuick3DUtils/private/qssgoption_p.h>
#include <QtQuick3DUtils/private/qssgdataref_p.h>

QT_BEGIN_NAMESPACE

struct QSSGPickResultProcessResult : public QSSGRenderPickResult
{
    QSSGPickResultProcessResult(const QSSGRenderPickResult &inSrc) : QSSGRenderPickResult(inSrc) {}
    QSSGPickResultProcessResult(const QSSGRenderPickResult &inSrc, bool consumed) : QSSGRenderPickResult(inSrc), m_wasPickConsumed(consumed) {}
    QSSGPickResultProcessResult() = default;
    bool m_wasPickConsumed = false;
};

class Q_QUICK3DRUNTIMERENDER_EXPORT QSSGRendererImpl : public QSSGRendererInterface
{
    typedef QHash<QSSGShaderDefaultMaterialKey, QSSGRef<QSSGShaderGeneratorGeneratedShader>> TShaderMap;
    typedef QHash<QByteArray, QSSGRef<QSSGRenderConstantBuffer>> TStrConstanBufMap;
    typedef QHash<const QSSGRenderLayer *, QSSGRef<QSSGLayerRenderData>> TInstanceRenderMap;
    typedef QVector<QSSGLayerRenderData *> TLayerRenderList;
    typedef QVector<QSSGRenderPickResult> TPickResultArray;

    // Items to implement the widget context.
    typedef QHash<QByteArray, QSSGRef<QSSGRenderVertexBuffer>> TStrVertBufMap;
    typedef QHash<QByteArray, QSSGRef<QSSGRenderIndexBuffer>> TStrIndexBufMap;
    typedef QHash<QByteArray, QSSGRef<QSSGRenderShaderProgram>> TStrShaderMap;
    typedef QHash<QByteArray, QSSGRef<QSSGRenderInputAssembler>> TStrIAMap;

    typedef QHash<long, QSSGRenderNode *> TBoneIdNodeMap;

    using PickResultList = QVarLengthArray<QSSGRenderPickResult, 20>; // Lets assume most items are filtered out already

    const QSSGRef<QSSGRenderContextInterface> m_contextInterface;
    QSSGRef<QSSGRenderContext> m_context;
    QSSGRef<QSSGBufferManager> m_bufferManager;
    // For rendering bounding boxes.
    QSSGRef<QSSGRenderVertexBuffer> m_boxVertexBuffer;
    QSSGRef<QSSGRenderIndexBuffer> m_boxIndexBuffer;
    QSSGRef<QSSGRenderShaderProgram> m_boxShader;
    QSSGRef<QSSGRenderShaderProgram> m_screenRectShader;

    QSSGRef<QSSGRenderVertexBuffer> m_axisVertexBuffer;
    QSSGRef<QSSGRenderShaderProgram> m_axisShader;

    // X,Y quad, broken down into 2 triangles and normalized over
    //-1,1.
    QSSGRef<QSSGRenderVertexBuffer> m_quadVertexBuffer;
    QSSGRef<QSSGRenderIndexBuffer> m_quadIndexBuffer;
    QSSGRef<QSSGRenderIndexBuffer> m_rectIndexBuffer;
    QSSGRef<QSSGRenderInputAssembler> m_quadInputAssembler;
    QSSGRef<QSSGRenderInputAssembler> m_rectInputAssembler;
    QSSGRef<QSSGRenderAttribLayout> m_quadAttribLayout;
    QSSGRef<QSSGRenderAttribLayout> m_rectAttribLayout;

    // X,Y triangle strip quads in screen coord dynamiclly setup
    QSSGRef<QSSGRenderVertexBuffer> m_quadStripVertexBuffer;
    QSSGRef<QSSGRenderInputAssembler> m_quadStripInputAssembler;
    QSSGRef<QSSGRenderAttribLayout> m_quadStripAttribLayout;

    // X,Y,Z point which is used for instanced based rendering of points
    QSSGRef<QSSGRenderVertexBuffer> m_pointVertexBuffer;
    QSSGRef<QSSGRenderInputAssembler> m_pointInputAssembler;
    QSSGRef<QSSGRenderAttribLayout> m_pointAttribLayout;

    QSSGRef<QSSGLayerSceneShader> m_sceneLayerShader;
    QSSGRef<QSSGLayerProgAABlendShader> m_layerProgAAShader;
    QSSGRef<QSSGLayerLastFrameBlendShader> m_layerLastFrameBlendShader;

    TShaderMap m_shaders;
    TStrConstanBufMap m_constantBuffers; ///< store the the shader constant buffers
    // Option is true if we have attempted to generate the shader.
    // This does not mean we were successul, however.
    QSSGRef<QSSGDefaultMaterialRenderableDepthShader> m_defaultMaterialDepthPrepassShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_depthPrepassShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_depthPrepassShaderDisplaced;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_depthTessLinearPrepassShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_depthTessLinearPrepassShaderDisplaced;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_depthTessPhongPrepassShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_depthTessNPatchPrepassShader;
    QSSGRef<QSSGSkyBoxShader> m_skyBoxShader;
    QSSGRef<QSSGDefaultAoPassShader> m_defaultAoPassShader;
    QSSGRef<QSSGDefaultAoPassShader> m_fakeDepthShader;
    QSSGRef<QSSGDefaultAoPassShader> m_fakeCubemapDepthShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_paraboloidDepthShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_paraboloidDepthTessLinearShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_paraboloidDepthTessPhongShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_paraboloidDepthTessNPatchShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_cubemapDepthShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_cubemapDepthTessLinearShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_cubemapDepthTessPhongShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_cubemapDepthTessNPatchShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_orthographicDepthShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_orthographicDepthTessLinearShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_orthographicDepthTessPhongShader;
    QSSGRef<QSSGRenderableDepthPrepassShader> m_orthographicDepthTessNPatchShader;
    QSSGRef<QSSGShadowmapPreblurShader> m_cubeShadowBlurXShader;
    QSSGRef<QSSGShadowmapPreblurShader> m_cubeShadowBlurYShader;
    QSSGRef<QSSGShadowmapPreblurShader> m_orthoShadowBlurXShader;
    QSSGRef<QSSGShadowmapPreblurShader> m_orthoShadowBlurYShader;

    // Overlay used to render all widgets.
    QRect m_beginFrameViewport;
    QSSGRef<QSSGRenderTexture2D> m_widgetTexture;
    QSSGRef<QSSGRenderFrameBuffer> m_widgetFbo;

    // Allocator for temporary data that is cleared after every layer.
    TInstanceRenderMap m_instanceRenderMap;
    TLayerRenderList m_lastFrameLayers;

    // Set from the first layer.
    TPickResultArray m_lastPickResults;

    // Temporary information stored only when rendering a particular layer.
    QSSGLayerRenderData *m_currentLayer;
    QMatrix4x4 m_viewProjection;
    QByteArray m_generatedShaderString;

    TStrVertBufMap m_widgetVertexBuffers;
    TStrIndexBufMap m_widgetIndexBuffers;
    TStrShaderMap m_widgetShaders;
    TStrIAMap m_widgetInputAssembler;

    TBoneIdNodeMap m_boneIdNodeMap;

    bool m_pickRenderPlugins;
    bool m_layerCachingEnabled;
    bool m_layerGPuProfilingEnabled;
    bool m_progressiveAARenderRequest;
    QSSGShaderDefaultMaterialKeyProperties m_defaultMaterialShaderKeyProperties;

    QSet<QSSGRenderGraphObject *> m_materialClearDirty;

public:
    QSSGRendererImpl(const QSSGRef<QSSGRenderContextInterface> &ctx);
    virtual ~QSSGRendererImpl() override;
    QSSGShaderDefaultMaterialKeyProperties &defaultMaterialShaderKeyProperties()
    {
        return m_defaultMaterialShaderKeyProperties;
    }

    void enableLayerCaching(bool inEnabled) override { m_layerCachingEnabled = inEnabled; }
    bool isLayerCachingEnabled() const override { return m_layerCachingEnabled; }

    void enableLayerGpuProfiling(bool inEnabled) override { m_layerGPuProfilingEnabled = inEnabled; }
    bool isLayerGpuProfilingEnabled() const override { return m_layerGPuProfilingEnabled; }

    // Calls prepare layer for render
    // and then do render layer.
    bool prepareLayerForRender(QSSGRenderLayer &inLayer, const QSize &surfaceSize) override;
    void renderLayer(QSSGRenderLayer &inLayer,
                     const QSize &surfaceSize,
                     bool clear,
                     const QColor &clearColor) override;
    void childrenUpdated(QSSGRenderNode &inParent) override;

    QSSGRenderCamera *cameraForNode(const QSSGRenderNode &inNode) const override;
    QSSGOption<QSSGCuboidRect> cameraBounds(const QSSGRenderGraphObject &inObject) override;
    virtual QSSGRenderLayer *layerForNode(const QSSGRenderNode &inNode) const;
    QSSGRef<QSSGLayerRenderData> getOrCreateLayerRenderDataForNode(const QSSGRenderNode &inNode);

    void beginFrame() override;
    void endFrame() override;

    void pickRenderPlugins(bool inPick) override { m_pickRenderPlugins = inPick; }
    QSSGRenderPickResult pick(QSSGRenderLayer &inLayer,
                                const QVector2D &inViewportDimensions,
                                const QVector2D &inMouseCoords,
                                bool inPickSiblings,
                                bool inPickEverything) override;
    QSSGRenderPickResult syncPick(QSSGRenderLayer &inLayer,
                                  const QVector2D &inViewportDimensions,
                                  const QVector2D &inMouseCoords,
                                  bool inPickSiblings,
                                  bool inPickEverything) override;

    virtual QSSGOption<QVector2D> facePosition(QSSGRenderNode &inNode,
                                                 QSSGBounds3 inBounds,
                                                 const QMatrix4x4 &inGlobalTransform,
                                                 const QVector2D &inViewportDimensions,
                                                 const QVector2D &inMouseCoords,
                                                 QSSGDataView<QSSGRenderGraphObject *> inMapperObjects,
                                                 QSSGRenderBasisPlanes inPlane) override;

    QVector3D unprojectToPosition(QSSGRenderNode &inNode, QVector3D &inPosition, const QVector2D &inMouseVec) const override;
    QVector3D unprojectWithDepth(QSSGRenderNode &inNode, QVector3D &inPosition, const QVector3D &inMouseVec) const override;
    QVector3D projectPosition(QSSGRenderNode &inNode, const QVector3D &inPosition) const override;

    QSSGOption<QSSGLayerPickSetup> getLayerPickSetup(QSSGRenderLayer &inLayer,
                                                         const QVector2D &inMouseCoords,
                                                         const QSize &inPickDims) override;

    QSSGOption<QRectF> layerRect(QSSGRenderLayer &inLayer) override;

    void runLayerRender(QSSGRenderLayer &inLayer, const QMatrix4x4 &inViewProjection) override;

    void renderLayerRect(QSSGRenderLayer &inLayer, const QVector3D &inColor) override;

    void releaseLayerRenderResources(QSSGRenderLayer &inLayer) override;

    void renderQuad(const QVector2D inDimensions, const QMatrix4x4 &inMVP, QSSGRenderTexture2D &inQuadTexture) override;
    void renderQuad() override;

    // render Gpu profiler values
    void dumpGpuProfilerStats() override;

    // Callback during the layer render process.
    void layerNeedsFrameClear(QSSGLayerRenderData &inLayer);
    void beginLayerDepthPassRender(QSSGLayerRenderData &inLayer);
    void endLayerDepthPassRender();
    void beginLayerRender(QSSGLayerRenderData &inLayer);
    void endLayerRender();
    void prepareImageForIbl(QSSGRenderImage &inImage);
    void addMaterialDirtyClear(QSSGRenderGraphObject *material);

    QSSGRef<QSSGRenderShaderProgram> compileShader(const QByteArray &inName, const char *inVert, const char *inFrame);

    QSSGRef<QSSGRenderShaderProgram> generateShader(QSSGSubsetRenderable &inRenderable, const ShaderFeatureSetList &inFeatureSet);
    QSSGRef<QSSGShaderGeneratorGeneratedShader> getShader(QSSGSubsetRenderable &inRenderable,
                                                              const ShaderFeatureSetList &inFeatureSet);

    QSSGRef<QSSGSkyBoxShader> getSkyBoxShader();
    QSSGRef<QSSGDefaultAoPassShader> getDefaultAoPassShader(const ShaderFeatureSetList &inFeatureSet);
    QSSGRef<QSSGDefaultAoPassShader> getFakeDepthShader(ShaderFeatureSetList inFeatureSet);
    QSSGRef<QSSGDefaultAoPassShader> getFakeCubeDepthShader(ShaderFeatureSetList inFeatureSet);
    QSSGRef<QSSGDefaultMaterialRenderableDepthShader> getRenderableDepthShader();

    QSSGRef<QSSGRenderableDepthPrepassShader> getParaboloidDepthShader(TessellationModeValues inTessMode);
    QSSGRef<QSSGRenderableDepthPrepassShader> getCubeShadowDepthShader(TessellationModeValues inTessMode);
    QSSGRef<QSSGRenderableDepthPrepassShader> getOrthographicDepthShader(TessellationModeValues inTessMode);

private:
    QSSGRef<QSSGRenderableDepthPrepassShader> getParaboloidDepthNoTessShader();
    QSSGRef<QSSGRenderableDepthPrepassShader> getParaboloidDepthTessLinearShader();
    QSSGRef<QSSGRenderableDepthPrepassShader> getParaboloidDepthTessPhongShader();
    QSSGRef<QSSGRenderableDepthPrepassShader> getParaboloidDepthTessNPatchShader();
    QSSGRef<QSSGRenderableDepthPrepassShader> getCubeDepthNoTessShader();
    QSSGRef<QSSGRenderableDepthPrepassShader> getCubeDepthTessLinearShader();
    QSSGRef<QSSGRenderableDepthPrepassShader> getCubeDepthTessPhongShader();
    QSSGRef<QSSGRenderableDepthPrepassShader> getCubeDepthTessNPatchShader();
    QSSGRef<QSSGRenderableDepthPrepassShader> getOrthographicDepthNoTessShader();
    QSSGRef<QSSGRenderableDepthPrepassShader> getOrthographicDepthTessLinearShader();
    QSSGRef<QSSGRenderableDepthPrepassShader> getOrthographicDepthTessPhongShader();
    QSSGRef<QSSGRenderableDepthPrepassShader> getOrthographicDepthTessNPatchShader();

public:
    const QSSGRef<QSSGRenderableDepthPrepassShader> &getDepthPrepassShader(bool inDisplaced);
    const QSSGRef<QSSGRenderableDepthPrepassShader> &getDepthTessPrepassShader(TessellationModeValues inTessMode, bool inDisplaced);
    const QSSGRef<QSSGRenderableDepthPrepassShader> &getDepthTessLinearPrepassShader(bool inDisplaced);
    const QSSGRef<QSSGRenderableDepthPrepassShader> &getDepthTessPhongPrepassShader();
    const QSSGRef<QSSGRenderableDepthPrepassShader> &getDepthTessNPatchPrepassShader();
    QSSGRef<QSSGLayerSceneShader> getSceneLayerShader();
    QSSGRef<QSSGRenderShaderProgram> getTextAtlasEntryShader();
    void generateXYQuad();
    void generateXYQuadStrip();
    void generateXYZPoint();
    QPair<QSSGRef<QSSGRenderVertexBuffer>, QSSGRef<QSSGRenderIndexBuffer>> getXYQuad();
    QSSGRef<QSSGLayerProgAABlendShader> getLayerProgAABlendShader();
    QSSGRef<QSSGLayerLastFrameBlendShader> getLayerLastFrameBlendShader();
    QSSGRef<QSSGShadowmapPreblurShader> getCubeShadowBlurXShader();
    QSSGRef<QSSGShadowmapPreblurShader> getCubeShadowBlurYShader();
    QSSGRef<QSSGShadowmapPreblurShader> getOrthoShadowBlurXShader();
    QSSGRef<QSSGShadowmapPreblurShader> getOrthoShadowBlurYShader();


    QSSGLayerRenderData *getLayerRenderData() { return m_currentLayer; }
    QSSGLayerGlobalRenderProperties getLayerGlobalRenderProperties();
    void updateCbAoShadow(const QSSGRenderLayer *pLayer, const QSSGRenderCamera *pCamera, QSSGResourceTexture2D &inDepthTexture);

    const QSSGRef<QSSGRenderContext> &context() { return m_context; }

    const QSSGRef<QSSGRenderContextInterface> &contextInterface() { return m_contextInterface; }

    void drawScreenRect(QRectF inRect, const QVector3D &inColor);
    // Binds an offscreen texture.  Widgets are rendered last.
    void setupWidgetLayer();

    // widget context implementation
    QSSGRef<QSSGRenderVertexBuffer> getOrCreateVertexBuffer(
            const QByteArray &inStr,
            quint32 stride,
            QSSGByteView bufferData = QSSGByteView());
    QSSGRef<QSSGRenderIndexBuffer> getOrCreateIndexBuffer(
            const QByteArray &inStr,
            QSSGRenderComponentType componentType,
            QSSGByteView bufferData = QSSGByteView());
    QSSGRef<QSSGRenderAttribLayout> createAttributeLayout(QSSGDataView<QSSGRenderVertexBufferEntry> attribs);
    QSSGRef<QSSGRenderInputAssembler> getOrCreateInputAssembler(const QByteArray &inStr,
                                                                    const QSSGRef<QSSGRenderAttribLayout> &attribLayout,
                                                                    QSSGDataView<QSSGRef<QSSGRenderVertexBuffer>> buffers,
                                                                    const QSSGRef<QSSGRenderIndexBuffer> indexBuffer,
                                                                    QSSGDataView<quint32> strides,
                                                                    QSSGDataView<quint32> offsets);

    QSSGRef<QSSGRenderVertexBuffer> getVertexBuffer(const QByteArray &inStr) const;
    QSSGRef<QSSGRenderIndexBuffer> getIndexBuffer(const QByteArray &inStr) const;
    QSSGRef<QSSGRenderInputAssembler> getInputAssembler(const QByteArray &inStr) const;

    QSSGRef<QSSGRenderShaderProgram> getShader(const QByteArray &inStr) const;
    QSSGRef<QSSGRenderShaderProgram> compileAndStoreShader(const QByteArray &inStr);
    const QSSGRef<QSSGShaderProgramGeneratorInterface> &getProgramGenerator();

    QSSGOption<QVector2D> getLayerMouseCoords(QSSGRenderLayer &inLayer,
                                                const QVector2D &inMouseCoords,
                                                const QVector2D &inViewportDimensions,
                                                bool forceImageIntersect = false) const override;

    bool rendererRequestsFrames() const override;

    static const QSSGRenderGraphObject *getPickObject(QSSGRenderableObject &inRenderableObject);
protected:
    QSSGOption<QVector2D> getLayerMouseCoords(QSSGLayerRenderData &inLayer,
                                                const QVector2D &inMouseCoords,
                                                const QVector2D &inViewportDimensions,
                                                bool forceImageIntersect = false) const;
    QSSGPickResultProcessResult processPickResultList(bool inPickEverything);
    // If the mouse y coordinates need to be flipped we expect that to happen before entry into
    // this function
    void getLayerHitObjectList(QSSGLayerRenderData &inLayer,
                               const QVector2D &inViewportDimensions,
                               const QVector2D &inMouseCoords,
                               bool inPickEverything,
                               TPickResultArray &outIntersectionResult);
    void getLayerHitObjectList(QSSGRenderLayer &layer,
                                const QVector2D &inViewportDimensions,
                                const QVector2D &inMouseCoords,
                                bool inPickEverything,
                                PickResultList &outIntersectionResult);
    static void intersectRayWithSubsetRenderable(const QSSGRef<QSSGBufferManager> &bufferManager,
                                                 const QSSGRenderRay &inRay,
                                                 const QSSGRenderNode &node,
                                                 PickResultList &outIntersectionResultList);
    static void intersectRayWithSubsetRenderable(const QSSGRenderRay &inRay,
                                          QSSGRenderableObject &inRenderableObject,
                                          TPickResultArray &outIntersectionResultList);
};
QT_END_NAMESPACE

#endif
