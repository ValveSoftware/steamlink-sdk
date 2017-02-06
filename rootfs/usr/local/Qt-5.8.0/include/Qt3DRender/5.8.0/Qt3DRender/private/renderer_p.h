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

#ifndef QT3DRENDER_RENDER_RENDERER_H
#define QT3DRENDER_RENDER_RENDERER_H

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

#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/private/shaderparameterpack_p.h>
#include <Qt3DRender/private/handle_types_p.h>
#include <Qt3DRender/private/abstractrenderer_p.h>
#include <Qt3DCore/qaspectjob.h>
#include <Qt3DRender/private/qt3drender_global_p.h>
#include <Qt3DRender/private/pickboundingvolumejob_p.h>
#include <Qt3DRender/private/rendersettings_p.h>
#include <Qt3DRender/private/renderviewinitializerjob_p.h>
#include <Qt3DRender/private/expandboundingvolumejob_p.h>
#include <Qt3DRender/private/updateworldtransformjob_p.h>
#include <Qt3DRender/private/calcboundingvolumejob_p.h>
#include <Qt3DRender/private/updateshaderdatatransformjob_p.h>
#include <Qt3DRender/private/framecleanupjob_p.h>
#include <Qt3DRender/private/updateworldboundingvolumejob_p.h>
#include <Qt3DRender/private/platformsurfacefilter_p.h>
#include <Qt3DRender/private/sendrendercapturejob_p.h>
#include <Qt3DRender/private/genericlambdajob_p.h>
#include <Qt3DRender/private/updatemeshtrianglelistjob_p.h>

#include <QHash>
#include <QMatrix4x4>
#include <QObject>

#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include <QScopedPointer>
#include <QSemaphore>

#include <functional>

QT_BEGIN_NAMESPACE

class QSurface;
class QMouseEvent;

namespace Qt3DCore {
class QEntity;
class QFrameAllocator;
class QEventFilterService;
}

namespace Qt3DRender {

class QCamera;
class QMaterial;
class QShaderProgram;
class QMesh;
class QRenderPass;
class QAbstractShapeMesh;
struct GraphicsApiFilterData;
class QSceneIOHandler;

#ifdef QT3D_JOBS_RUN_STATS
namespace Debug {
class CommandExecuter;
}
#endif

namespace Render {

class CameraLens;
class GraphicsContext;
class FrameGraphNode;
class Material;
class Technique;
class Shader;
class Entity;
class RenderCommand;
class RenderQueue;
class RenderView;
class Effect;
class RenderPass;
class RenderThread;
class RenderStateSet;
class VSyncFrameAdvanceService;
class PickEventFilter;
class NodeManagers;

using SynchronizerJobPtr = GenericLambdaJobPtr<std::function<void()>>;

class QT3DRENDERSHARED_PRIVATE_EXPORT Renderer : public AbstractRenderer
{
public:
    explicit Renderer(QRenderAspect::RenderType type);
    ~Renderer();

    void dumpInfo() const Q_DECL_OVERRIDE;
    API api() const Q_DECL_OVERRIDE { return AbstractRenderer::OpenGL; }

    qint64 time() const Q_DECL_OVERRIDE;
    void setTime(qint64 time) Q_DECL_OVERRIDE;

    void setNodeManagers(NodeManagers *managers) Q_DECL_OVERRIDE;
    void setServices(Qt3DCore::QServiceLocator *services) Q_DECL_OVERRIDE { m_services = services; }
    void setSurfaceExposed(bool exposed) Q_DECL_OVERRIDE;

    NodeManagers *nodeManagers() const Q_DECL_OVERRIDE;
    Qt3DCore::QServiceLocator *services() const Q_DECL_OVERRIDE { return m_services; }

    void initialize() Q_DECL_OVERRIDE;
    void shutdown() Q_DECL_OVERRIDE;
    void releaseGraphicsResources() Q_DECL_OVERRIDE;

    void render() Q_DECL_OVERRIDE;
    void doRender() Q_DECL_OVERRIDE;
    void cleanGraphicsResources() Q_DECL_OVERRIDE;

    bool isRunning() const Q_DECL_OVERRIDE { return m_running.load(); }

    void setSceneRoot(Qt3DCore::QBackendNodeFactory *factory, Entity *sgRoot) Q_DECL_OVERRIDE;
    Entity *sceneRoot() const Q_DECL_OVERRIDE { return m_renderSceneRoot; }

    FrameGraphNode *frameGraphRoot() const Q_DECL_OVERRIDE;

    void markDirty(BackendNodeDirtySet changes, BackendNode *node) Q_DECL_OVERRIDE;
    BackendNodeDirtySet dirtyBits() Q_DECL_OVERRIDE;
    void clearDirtyBits(BackendNodeDirtySet changes) Q_DECL_OVERRIDE;


    bool shouldRender() Q_DECL_OVERRIDE;
    void skipNextFrame() Q_DECL_OVERRIDE;

    QVector<Qt3DCore::QAspectJobPtr> renderBinJobs() Q_DECL_OVERRIDE;
    Qt3DCore::QAspectJobPtr pickBoundingVolumeJob() Q_DECL_OVERRIDE;
    Qt3DCore::QAspectJobPtr syncTextureLoadingJob() Q_DECL_OVERRIDE;

    QVector<Qt3DCore::QAspectJobPtr> createRenderBufferJobs() const;

    inline FrameCleanupJobPtr frameCleanupJob() const { return m_cleanupJob; }
    inline ExpandBoundingVolumeJobPtr expandBoundingVolumeJob() const { return m_expandBoundingVolumeJob; }
    inline UpdateShaderDataTransformJobPtr updateShaderDataTransformJob() const { return m_updateShaderDataTransformJob; }
    inline CalculateBoundingVolumeJobPtr calculateBoundingVolumeJob() const { return m_calculateBoundingVolumeJob; }
    inline UpdateWorldTransformJobPtr updateWorldTransformJob() const { return m_worldTransformJob; }
    inline UpdateWorldBoundingVolumeJobPtr updateWorldBoundingVolumeJob() const { return m_updateWorldBoundingVolumeJob; }
    inline UpdateMeshTriangleListJobPtr updateMeshTriangleListJob() const { return m_updateMeshTriangleListJob; }
    inline SynchronizerJobPtr textureLoadSyncJob() const { return m_syncTextureLoadingJob; }

    Qt3DCore::QAbstractFrameAdvanceService *frameAdvanceService() const Q_DECL_OVERRIDE;

    void registerEventFilter(Qt3DCore::QEventFilterService *service) Q_DECL_OVERRIDE;

    virtual void setSettings(RenderSettings *settings) Q_DECL_OVERRIDE;
    virtual RenderSettings *settings() const Q_DECL_OVERRIDE;

    void updateGLResources();
    void updateTexture(Texture *texture);
    void cleanupTexture(const Texture *texture);

    void prepareCommandsSubmission(const QVector<RenderView *> &renderViews);
    bool executeCommandsSubmission(const RenderView *rv);
    void updateVAOWithAttributes(Geometry *geometry,
                                 RenderCommand *command,
                                 Shader *shader,
                                 bool forceUpdate);

    bool requiresVAOAttributeUpdate(Geometry *geometry,
                                    RenderCommand *command) const;

    void setOpenGLContext(QOpenGLContext *context);
    const GraphicsApiFilterData *contextInfo() const;
    GraphicsContext *graphicsContext() const;

    inline RenderStateSet *defaultRenderState() const { return m_defaultRenderStateSet; }


    QList<QMouseEvent> pendingPickingEvents() const;

    void addRenderCaptureSendRequest(Qt3DCore::QNodeId nodeId);
    const QVector<Qt3DCore::QNodeId> takePendingRenderCaptureSendRequests();

    void enqueueRenderView(RenderView *renderView, int submitOrder);
    bool isReadyToSubmit();

    QVariant executeCommand(const QStringList &args) Q_DECL_OVERRIDE;
    void setOffscreenSurfaceHelper(OffscreenSurfaceHelper *helper) Q_DECL_OVERRIDE;
    QSurfaceFormat format() Q_DECL_OVERRIDE;

    struct ViewSubmissionResultData
    {
        ViewSubmissionResultData()
            : lastBoundFBOId(0)
            , surface(nullptr)
        {}

        uint lastBoundFBOId;
        QSurface *surface;
    };

    ViewSubmissionResultData submitRenderViews(const QVector<Render::RenderView *> &renderViews);

    QMutex* mutex() { return &m_mutex; }


#ifdef QT3D_RENDER_UNIT_TESTS
public:
#else
private:
#endif
    bool canRender() const;

    Qt3DCore::QServiceLocator *m_services;
    NodeManagers *m_nodesManager;

    // Frame graph root
    Qt3DCore::QNodeId m_frameGraphRootUuid;

    Entity *m_renderSceneRoot;

    // Fail safe values that we can use if a RenderCommand
    // is missing a shader
    RenderStateSet *m_defaultRenderStateSet;
    ShaderParameterPack m_defaultUniformPack;

    QScopedPointer<GraphicsContext> m_graphicsContext;
    QSurfaceFormat m_format;

    RenderQueue *m_renderQueue;
    QScopedPointer<RenderThread> m_renderThread;
    QScopedPointer<VSyncFrameAdvanceService> m_vsyncFrameAdvanceService;

    QMutex m_mutex;
    QSemaphore m_submitRenderViewsSemaphore;
    QSemaphore m_waitForInitializationToBeCompleted;

    QAtomicInt m_running;

    QScopedPointer<PickEventFilter> m_pickEventFilter;

    QVector<Attribute *> m_dirtyAttributes;
    QVector<Geometry *> m_dirtyGeometry;
    QAtomicInt m_exposed;
    BackendNodeDirtySet m_changeSet;
    QAtomicInt m_lastFrameCorrect;
    QOpenGLContext *m_glContext;
    PickBoundingVolumeJobPtr m_pickBoundingVolumeJob;

    qint64 m_time;

    RenderSettings *m_settings;

    UpdateShaderDataTransformJobPtr m_updateShaderDataTransformJob;
    FrameCleanupJobPtr m_cleanupJob;
    UpdateWorldTransformJobPtr m_worldTransformJob;
    ExpandBoundingVolumeJobPtr m_expandBoundingVolumeJob;
    CalculateBoundingVolumeJobPtr m_calculateBoundingVolumeJob;
    UpdateWorldBoundingVolumeJobPtr m_updateWorldBoundingVolumeJob;
    SendRenderCaptureJobPtr m_sendRenderCaptureJob;
    UpdateMeshTriangleListJobPtr m_updateMeshTriangleListJob;

    QVector<Qt3DCore::QNodeId> m_pendingRenderCaptureSendRequests;

    void performDraw(RenderCommand *command);
    void performCompute(const RenderView *rv, RenderCommand *command);
    void createOrUpdateVAO(RenderCommand *command,
                           HVao *previousVAOHandle,
                           OpenGLVertexArrayObject **vao);

    GenericLambdaJobPtr<std::function<void ()>> m_bufferGathererJob;
    GenericLambdaJobPtr<std::function<void ()>> m_textureGathererJob;
    GenericLambdaJobPtr<std::function<void ()>> m_shaderGathererJob;

    SynchronizerJobPtr m_syncTextureLoadingJob;

    void lookForDirtyBuffers();
    void lookForDirtyTextures();
    void lookForDirtyShaders();

    QVector<HBuffer> m_dirtyBuffers;
    QVector<HShader> m_dirtyShaders;
    QVector<HTexture> m_dirtyTextures;

    bool m_ownedContext;

    OffscreenSurfaceHelper *m_offscreenHelper;
    QMutex m_offscreenSurfaceMutex;

#ifdef QT3D_JOBS_RUN_STATS
    QScopedPointer<Qt3DRender::Debug::CommandExecuter> m_commandExecuter;
    friend class Qt3DRender::Debug::CommandExecuter;
#endif

    QMetaObject::Connection m_contextConnection;
};

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_RENDER_RENDERER_H
