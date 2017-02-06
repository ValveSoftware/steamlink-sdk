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

#include "renderer_p.h"

#include <Qt3DCore/qentity.h>

#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qmesh.h>
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qshaderprogram.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DRender/qeffect.h>

#include <Qt3DRender/private/qsceneiohandler_p.h>
#include <Qt3DRender/private/renderstates_p.h>
#include <Qt3DRender/private/cameraselectornode_p.h>
#include <Qt3DRender/private/framegraphvisitor_p.h>
#include <Qt3DRender/private/graphicscontext_p.h>
#include <Qt3DRender/private/cameralens_p.h>
#include <Qt3DRender/private/rendercommand_p.h>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/private/renderlogging_p.h>
#include <Qt3DRender/private/material_p.h>
#include <Qt3DRender/private/renderpassfilternode_p.h>
#include <Qt3DRender/private/renderqueue_p.h>
#include <Qt3DRender/private/shader_p.h>
#include <Qt3DRender/private/buffer_p.h>
#include <Qt3DRender/private/renderstateset_p.h>
#include <Qt3DRender/private/technique_p.h>
#include <Qt3DRender/private/renderthread_p.h>
#include <Qt3DRender/private/renderview_p.h>
#include <Qt3DRender/private/techniquefilternode_p.h>
#include <Qt3DRender/private/viewportnode_p.h>
#include <Qt3DRender/private/vsyncframeadvanceservice_p.h>
#include <Qt3DRender/private/pickeventfilter_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/buffermanager_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/gltexturemanager_p.h>
#include <Qt3DRender/private/gltexture_p.h>
#include <Qt3DRender/private/geometryrenderermanager_p.h>
#include <Qt3DRender/private/openglvertexarrayobject_p.h>
#include <Qt3DRender/private/platformsurfacefilter_p.h>
#include <Qt3DRender/private/loadbufferjob_p.h>
#include <Qt3DRender/private/rendercapture_p.h>
#include <Qt3DRender/private/offscreensurfacehelper_p.h>

#include <Qt3DRender/qcameralens.h>
#include <Qt3DCore/private/qeventfilterservice_p.h>
#include <Qt3DCore/private/qabstractaspectjobmanager_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/private/aspectcommanddebugger_p.h>

#include <QStack>
#include <QOffscreenSurface>
#include <QSurface>
#include <QElapsedTimer>
#include <QLibraryInfo>
#include <QPluginLoader>
#include <QDir>
#include <QUrl>
#include <QOffscreenSurface>
#include <QWindow>

#include <QtGui/private/qopenglcontext_p.h>

// For Debug purposes only
#include <QThread>


#ifdef QT3D_JOBS_RUN_STATS
#include <Qt3DCore/private/qthreadpooler_p.h>
#include <Qt3DRender/private/job_common_p.h>
#include <Qt3DRender/private/commandexecuter_p.h>
#endif

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

/*!
    \internal

    Renderer shutdown procedure:

    Since the renderer relies on the surface and OpenGLContext to perform its cleanup,
    it is shutdown when the surface is set to nullptr

    When the surface is set to nullptr this will request the RenderThread to terminate
    and will prevent createRenderBinJobs from returning a set of jobs as there is nothing
    more to be rendered.

    In turn, this will call shutdown which will make the OpenGL context current one last time
    to allow cleanups requiring a call to QOpenGLContext::currentContext to execute properly.
    At the end of that function, the GraphicsContext is set to null.

    At this point though, the QAspectThread is still running its event loop and will only stop
    a short while after.
 */

Renderer::Renderer(QRenderAspect::RenderType type)
    : m_services(nullptr)
    , m_nodesManager(nullptr)
    , m_defaultRenderStateSet(nullptr)
    , m_graphicsContext(nullptr)
    , m_renderQueue(new RenderQueue())
    , m_renderThread(type == QRenderAspect::Threaded ? new RenderThread(this) : nullptr)
    , m_vsyncFrameAdvanceService(new VSyncFrameAdvanceService())
    , m_waitForInitializationToBeCompleted(0)
    , m_pickEventFilter(new PickEventFilter())
    , m_exposed(0)
    , m_changeSet(0)
    , m_lastFrameCorrect(0)
    , m_glContext(nullptr)
    , m_pickBoundingVolumeJob(PickBoundingVolumeJobPtr::create())
    , m_time(0)
    , m_settings(nullptr)
    , m_updateShaderDataTransformJob(Render::UpdateShaderDataTransformJobPtr::create())
    , m_cleanupJob(Render::FrameCleanupJobPtr::create())
    , m_worldTransformJob(Render::UpdateWorldTransformJobPtr::create())
    , m_expandBoundingVolumeJob(Render::ExpandBoundingVolumeJobPtr::create())
    , m_calculateBoundingVolumeJob(Render::CalculateBoundingVolumeJobPtr::create())
    , m_updateWorldBoundingVolumeJob(Render::UpdateWorldBoundingVolumeJobPtr::create())
    , m_sendRenderCaptureJob(Render::SendRenderCaptureJobPtr::create(this))
    , m_updateMeshTriangleListJob(Render::UpdateMeshTriangleListJobPtr::create())
    , m_bufferGathererJob(Render::GenericLambdaJobPtr<std::function<void ()>>::create([this] { lookForDirtyBuffers(); }, JobTypes::DirtyBufferGathering))
    , m_textureGathererJob(Render::GenericLambdaJobPtr<std::function<void ()>>::create([this] { lookForDirtyTextures(); }, JobTypes::DirtyTextureGathering))
    , m_shaderGathererJob(Render::GenericLambdaJobPtr<std::function<void ()>>::create([this] { lookForDirtyShaders(); }, JobTypes::DirtyShaderGathering))
    , m_syncTextureLoadingJob(Render::GenericLambdaJobPtr<std::function<void ()>>::create([] {}, JobTypes::SyncTextureLoading))
    , m_ownedContext(false)
    , m_offscreenHelper(nullptr)
    #ifdef QT3D_JOBS_RUN_STATS
    , m_commandExecuter(new Qt3DRender::Debug::CommandExecuter(this))
    #endif
{
    // Set renderer as running - it will wait in the context of the
    // RenderThread for RenderViews to be submitted
    m_running.fetchAndStoreOrdered(1);
    if (m_renderThread)
        m_renderThread->waitForStart();

    // Create jobs to update transforms and bounding volumes
    // We can only update bounding volumes once all world transforms are known
    m_updateWorldBoundingVolumeJob->addDependency(m_worldTransformJob);
    m_updateWorldBoundingVolumeJob->addDependency(m_calculateBoundingVolumeJob);
    m_expandBoundingVolumeJob->addDependency(m_updateWorldBoundingVolumeJob);
    m_updateShaderDataTransformJob->addDependency(m_worldTransformJob);

    // Dirty texture gathering depends on m_syncTextureLoadingJob
    // m_syncTextureLoadingJob will depend on the texture loading jobs
    m_textureGathererJob->addDependency(m_syncTextureLoadingJob);

    // All world stuff depends on the RenderEntity's localBoundingVolume
    m_pickBoundingVolumeJob->addDependency(m_updateMeshTriangleListJob);

    m_defaultRenderStateSet = new RenderStateSet;
    m_defaultRenderStateSet->addState(RenderStateSet::createState<DepthTest>(GL_LESS));
    m_defaultRenderStateSet->addState(RenderStateSet::createState<CullFace>(GL_BACK));
    m_defaultRenderStateSet->addState(RenderStateSet::createState<ColorMask>(true, true, true, true));
}

Renderer::~Renderer()
{
    // If using a threaded rendering approach, tell the thread to exit
    // and wait for it to be done
    m_running.fetchAndStoreOrdered(0);
    if (m_renderThread)
        m_renderThread->wait();

    delete m_renderQueue;
    delete m_defaultRenderStateSet;

    if (!m_ownedContext)
        QObject::disconnect(m_contextConnection);
}

void Renderer::dumpInfo() const
{
    qDebug() << Q_FUNC_INFO << "t =" << m_time;

    const ShaderManager *shaderManager = m_nodesManager->shaderManager();
    qDebug() << "=== Shader Manager ===";
    qDebug() << *shaderManager;
}

qint64 Renderer::time() const
{
    return m_time;
}

void Renderer::setTime(qint64 time)
{
    m_time = time;
}

void Renderer::setNodeManagers(NodeManagers *managers)
{
    m_nodesManager = managers;

    m_updateShaderDataTransformJob->setManagers(m_nodesManager);
    m_cleanupJob->setManagers(m_nodesManager);
    m_calculateBoundingVolumeJob->setManagers(m_nodesManager);
    m_pickBoundingVolumeJob->setManagers(m_nodesManager);
    m_updateWorldBoundingVolumeJob->setManager(m_nodesManager->renderNodesManager());
    m_sendRenderCaptureJob->setManagers(m_nodesManager);
    m_updateMeshTriangleListJob->setManagers(m_nodesManager);
}

NodeManagers *Renderer::nodeManagers() const
{
    return m_nodesManager;
}

void Renderer::setOpenGLContext(QOpenGLContext *context)
{
    m_glContext = context;
}

// Called in RenderThread context by the run method of RenderThread
// RenderThread has locked the mutex already and unlocks it when this
// method termintates
void Renderer::initialize()
{
    m_graphicsContext.reset(new GraphicsContext);
    m_graphicsContext->setRenderer(this);

    QOpenGLContext* ctx = m_glContext;

    // If we are using our own context (not provided by QtQuick),
    // we need to create it
    if (!m_glContext) {
        ctx = new QOpenGLContext;
        ctx->setShareContext(qt_gl_global_share_context());

        // TO DO: Shouldn't we use the highest context available and trust
        // QOpenGLContext to fall back on the best lowest supported ?
        const QByteArray debugLoggingMode = qgetenv("QT3DRENDER_DEBUG_LOGGING");

        if (!debugLoggingMode.isEmpty()) {
            QSurfaceFormat sf = ctx->format();
            sf.setOption(QSurfaceFormat::DebugContext);
            ctx->setFormat(sf);
        }

        // Create OpenGL context
        if (ctx->create())
            qCDebug(Backend) << "OpenGL context created with actual format" << ctx->format();
        else
            qCWarning(Backend) << Q_FUNC_INFO << "OpenGL context creation failed";
        m_ownedContext = true;
    } else {
        // Context is not owned by us, so we need to know if it gets destroyed
        m_contextConnection = QObject::connect(m_glContext, &QOpenGLContext::aboutToBeDestroyed,
                                               [this] { releaseGraphicsResources(); });
    }

    // Note: we don't have a surface at this point
    // The context will be made current later on (at render time)
    m_graphicsContext->setOpenGLContext(ctx);

    // Store the format used by the context and queue up creating an
    // offscreen surface in the main thread so that it is available
    // for use when we want to shutdown the renderer. We need to create
    // the offscreen surface on the main thread because on some platforms
    // (MS Windows), an offscreen surface is just a hidden QWindow.
    m_format = ctx->format();
    QMetaObject::invokeMethod(m_offscreenHelper, "createOffscreenSurface");

    // Awake setScenegraphRoot in case it was waiting
    m_waitForInitializationToBeCompleted.release(1);
    // Allow the aspect manager to proceed
    m_vsyncFrameAdvanceService->proceedToNextFrame();
}

/*!
 * \internal
 *
 * Signals for the renderer to stop rendering. If a threaded renderer is in use,
 * the render thread will call releaseGraphicsResources() just before the thread exits.
 * If rendering synchronously, this function will call releaseGraphicsResources().
 */
void Renderer::shutdown()
{
    qCDebug(Backend) << Q_FUNC_INFO << "Requesting renderer shutdown";
    m_running.store(0);

    // We delete any renderqueue that we may not have had time to render
    // before the surface was destroyed
    qDeleteAll(m_renderQueue->nextFrameQueue());
    m_renderQueue->reset();

    if (!m_renderThread) {
        releaseGraphicsResources();
    } else {
        // Wake up the render thread in case it is waiting for some renderviews
        // to be ready. The isReadyToSubmit() function checks for a shutdown
        // having been requested.
        m_submitRenderViewsSemaphore.release(1);
    }
}

/*!
    \internal

    When using a threaded renderer this function is called in the context of the
    RenderThread to do any shutdown and cleanup that needs to be performed in the
    thread where the OpenGL context lives.

    When using Scene3D or anything that provides a custom QOpenGLContext (not
    owned by Qt3D) this function is called whenever the signal
    QOpenGLContext::aboutToBeDestroyed is emitted. In that case this function
    is called in the context of the emitter's thread.
*/
void Renderer::releaseGraphicsResources()
{
    // We may get called twice when running inside of a Scene3D. Once when Qt Quick
    // wants to shutdown, and again when the render aspect gets unregistered. So
    // check that we haven't already cleaned up before going any further.
    if (!m_graphicsContext)
        return;

    // Try to temporarily make the context current so we can free up any resources
    QMutexLocker locker(&m_offscreenSurfaceMutex);
    QOffscreenSurface *offscreenSurface = m_offscreenHelper->offscreenSurface();
    if (!offscreenSurface) {
        qWarning() << "Failed to make context current: OpenGL resources will not be destroyed";
        return;
    }

    QOpenGLContext *context = m_graphicsContext->openGLContext();
    Q_ASSERT(context);
    if (context->makeCurrent(offscreenSurface)) {

        // Clean up the graphics context and any resources
        const QVector<GLTexture*> activeTextures = m_nodesManager->glTextureManager()->activeResources();
        for (GLTexture *tex : activeTextures)
            tex->destroyGLTexture();

        // TO DO: Do the same thing with buffers

        context->doneCurrent();
    } else {
        qWarning() << "Failed to make context current: OpenGL resources will not be destroyed";
    }

    if (m_ownedContext)
        delete context;

    m_graphicsContext.reset(nullptr);
    qCDebug(Backend) << Q_FUNC_INFO << "Renderer properly shutdown";
}

void Renderer::setSurfaceExposed(bool exposed)
{
    qCDebug(Backend) << "Window exposed: " << exposed;
    m_exposed.fetchAndStoreOrdered(exposed);
}

Render::FrameGraphNode *Renderer::frameGraphRoot() const
{
    Q_ASSERT(m_settings);
    return m_nodesManager->frameGraphManager()->lookupNode(m_settings->activeFrameGraphID());
}

// QAspectThread context
// Order of execution :
// 1) RenderThread is created -> release 1 of m_waitForInitializationToBeCompleted when started
// 2) setSceneRoot waits to acquire initialization
// 3) submitRenderView -> check for surface
//    -> make surface current + create proper glHelper if needed
void Renderer::setSceneRoot(QBackendNodeFactory *factory, Entity *sgRoot)
{
    Q_ASSERT(sgRoot);
    Q_UNUSED(factory);

    // If initialization hasn't been completed we must wait
    m_waitForInitializationToBeCompleted.acquire();

    m_renderSceneRoot = sgRoot;
    if (!m_renderSceneRoot)
        qCWarning(Backend) << "Failed to build render scene";
    m_renderSceneRoot->dump();
    qCDebug(Backend) << Q_FUNC_INFO << "DUMPING SCENE";

    // Set the scene root on the jobs
    m_worldTransformJob->setRoot(m_renderSceneRoot);
    m_expandBoundingVolumeJob->setRoot(m_renderSceneRoot);
    m_calculateBoundingVolumeJob->setRoot(m_renderSceneRoot);
    m_cleanupJob->setRoot(m_renderSceneRoot);
    m_pickBoundingVolumeJob->setRoot(m_renderSceneRoot);
}

void Renderer::registerEventFilter(QEventFilterService *service)
{
    qCDebug(Backend) << Q_FUNC_INFO << QThread::currentThread();
    service->registerEventFilter(m_pickEventFilter.data(), 1024);
}

void Renderer::setSettings(RenderSettings *settings)
{
    m_settings = settings;
}

RenderSettings *Renderer::settings() const
{
    return m_settings;
}

void Renderer::render()
{
    // Traversing the framegraph tree from root to lead node
    // Allows us to define the rendering set up
    // Camera, RenderTarget ...

    // Utimately the renderer should be a framework
    // For the processing of the list of renderviews

    // Matrice update, bounding volumes computation ...
    // Should be jobs

    // namespace Qt3DCore has 2 distincts node trees
    // One scene description
    // One framegraph description

    while (m_running.load() > 0) {
        doRender();
        // TO DO: Restore windows exposed detection
        // Probably needs to happens some place else though
    }
}

void Renderer::doRender()
{
    bool submissionSucceeded = false;
    bool hasCleanedQueueAndProceeded = false;
    Renderer::ViewSubmissionResultData submissionData;
    bool preprocessingComplete = false;

    if (isReadyToSubmit()) {

        // Lock the mutex to protect access to m_surface and check if we are still set
        // to the running state and that we have a valid surface on which to draw
        // TO DO: Is that still needed given the surface changes
        QMutexLocker locker(&m_mutex);
        const QVector<Render::RenderView *> renderViews = m_renderQueue->nextFrameQueue();

#ifdef QT3D_JOBS_RUN_STATS
        // Save start of frame
        JobRunStats submissionStatsPart1;
        JobRunStats submissionStatsPart2;
        submissionStatsPart1.jobId.typeAndInstance[0] = JobTypes::FrameSubmissionPart1;
        submissionStatsPart1.jobId.typeAndInstance[1] = 0;
        submissionStatsPart1.threadId = reinterpret_cast<quint64>(QThread::currentThreadId());
        submissionStatsPart1.startTime = QThreadPooler::m_jobsStatTimer.nsecsElapsed();
        submissionStatsPart2.jobId.typeAndInstance[0] = JobTypes::FrameSubmissionPart2;
        submissionStatsPart2.jobId.typeAndInstance[1] = 0;
        submissionStatsPart2.threadId = reinterpret_cast<quint64>(QThread::currentThreadId());
#endif

        if (canRender() && (submissionSucceeded = renderViews.size() > 0) == true) {
            // Clear all dirty flags but Compute so that
            // we still render every frame when a compute shader is used in a scene
            BackendNodeDirtySet changesToUnset = m_changeSet;
            if (changesToUnset.testFlag(Renderer::ComputeDirty))
                changesToUnset.setFlag(Renderer::ComputeDirty, false);
            clearDirtyBits(changesToUnset);

            { // Scoped to destroy surfaceLock
                QSurface *surface = nullptr;
                for (const Render::RenderView *rv: renderViews) {
                    surface = rv->surface();
                    if (surface)
                        break;
                }

                SurfaceLocker surfaceLock(surface);
                const bool surfaceIsValid = (surface && surfaceLock.isSurfaceValid());
                if (surfaceIsValid) {
                    // Reset state for each draw if we don't have complete control of the context
                    if (!m_ownedContext)
                        m_graphicsContext->setCurrentStateSet(nullptr);
                    if (m_graphicsContext->beginDrawing(surface)) {
                        // 1) Execute commands for buffer uploads, texture updates, shader loading first
                        updateGLResources();
                        // 2) Update VAO and copy data into commands to allow concurrent submission
                        prepareCommandsSubmission(renderViews);
                        preprocessingComplete = true;
                    }
                }
            }
            // 2) Proceed to next frame and start preparing frame n + 1
            m_renderQueue->reset();
            m_vsyncFrameAdvanceService->proceedToNextFrame();
            hasCleanedQueueAndProceeded = true;

#ifdef QT3D_JOBS_RUN_STATS
            if (preprocessingComplete) {
                submissionStatsPart2.startTime = QThreadPooler::m_jobsStatTimer.nsecsElapsed();
                submissionStatsPart1.endTime = submissionStatsPart2.startTime;
            }
#endif
            // Only try to submit the RenderViews if the preprocessing was successful
            // This part of the submission is happening in parallel to the RV building for the next frame
            if (preprocessingComplete) {
                // 3) Submit the render commands for frame n (making sure we never reference something that could be changing)
                // Render using current device state and renderer configuration
                submissionData = submitRenderViews(renderViews);

                // Perform any required cleanup of the Graphics resources (Buffers deleted, Shader deleted...)
                cleanGraphicsResources();
            }
        }

#ifdef QT3D_JOBS_RUN_STATS
        // Execute the pending shell commands
        m_commandExecuter->performAsynchronousCommandExecution(renderViews);
#endif

        // Delete all the RenderViews which will clear the allocators
        // that were used for their allocation
        qDeleteAll(renderViews);

#ifdef QT3D_JOBS_RUN_STATS
        if (preprocessingComplete) {
            // Save submission elapsed time
            submissionStatsPart2.endTime = QThreadPooler::m_jobsStatTimer.nsecsElapsed();
            // Note this is safe since proceedToNextFrame is the one going to trigger
            // the write to the file, and this is performed after this step
            Qt3DCore::QThreadPooler::addSubmissionLogStatsEntry(submissionStatsPart1);
            Qt3DCore::QThreadPooler::addSubmissionLogStatsEntry(submissionStatsPart2);
        }
#endif
    }

    // Note: submissionSucceeded is false when
    // * we cannot render because a shutdown has been scheduled
    // * the renderqueue is incomplete (only when rendering with a Scene3D)
    // Otherwise returns true even for cases like
    // * No render view
    // * No surface set
    // * OpenGLContext failed to be set current
    // This behavior is important as we need to
    // call proceedToNextFrame despite rendering errors that aren't fatal

    // Only reset renderQueue and proceed to next frame if the submission
    // succeeded or it we are using a render thread and that is wasn't performed
    // already

    // If submissionSucceeded isn't true this implies that something went wrong
    // with the rendering and/or the renderqueue is incomplete from some reason
    // (in the case of scene3d the render jobs may be taking too long ....)
    if (m_renderThread || submissionSucceeded) {

        if (!hasCleanedQueueAndProceeded) {
            // Reset the m_renderQueue so that we won't try to render
            // with a queue used by a previous frame with corrupted content
            // if the current queue was correctly submitted
            m_renderQueue->reset();

            // We allow the RenderTickClock service to proceed to the next frame
            // In turn this will allow the aspect manager to request a new set of jobs
            // to be performed for each aspect
            m_vsyncFrameAdvanceService->proceedToNextFrame();
        }

        // Perform the last swapBuffers calls after the proceedToNextFrame
        // as this allows us to gain a bit of time for the preparation of the
        // next frame
        // Finish up with last surface used in the list of RenderViews
        if (submissionSucceeded) {
            SurfaceLocker surfaceLock(submissionData.surface);
            // Finish up with last surface used in the list of RenderViews
            m_graphicsContext->endDrawing(submissionData.lastBoundFBOId == m_graphicsContext->defaultFBO() && surfaceLock.isSurfaceValid());
        }
    }
}

// Called by RenderViewJobs
void Renderer::enqueueRenderView(Render::RenderView *renderView, int submitOrder)
{
    QMutexLocker locker(&m_mutex); // Prevent out of order execution
    // We cannot use a lock free primitive here because:
    // - QVector is not thread safe
    // - Even if the insert is made correctly, the isFrameComplete call
    //   could be invalid since depending on the order of execution
    //   the counter could be complete but the renderview not yet added to the
    //   buffer depending on whichever order the cpu decides to process this

    if (m_renderQueue->queueRenderView(renderView, submitOrder)) {
        if (m_renderThread && m_running.load())
            Q_ASSERT(m_submitRenderViewsSemaphore.available() == 0);
        m_submitRenderViewsSemaphore.release(1);
    }
}

bool Renderer::canRender() const
{
    // Make sure that we've not been told to terminate whilst waiting on
    // the above wait condition
    if (m_renderThread && !m_running.load()) {
        qCDebug(Rendering) << "RenderThread termination requested whilst waiting";
        return false;
    }

    // TO DO: Check if all surfaces have been destroyed...
    // It may be better if the last window to be closed trigger a call to shutdown
    // Rather than having checks for the surface everywhere

    return true;
}

bool Renderer::isReadyToSubmit()
{
    // If we are using a render thread, make sure that
    // we've been told to render before rendering
    if (m_renderThread) { // Prevent ouf of order execution
        m_submitRenderViewsSemaphore.acquire(1);

        // Check if shutdown has been requested
        if (m_running.load() == 0)
            return false;

        // When using Thread rendering, the semaphore should only
        // be released when the frame queue is complete and there's
        // something to render
        // The case of shutdown should have been handled just before
        Q_ASSERT(m_renderQueue->isFrameQueueComplete());
    } else {
        // When using synchronous rendering (QtQuick)
        // We are not sure that the frame queue is actually complete
        // Since a call to render may not be synched with the completions
        // of the RenderViewJobs
        // In such a case we return early, waiting for a next call with
        // the frame queue complete at this point
        QMutexLocker locker(&m_mutex);
        if (!m_renderQueue->isFrameQueueComplete())
            return false;
    }
    return true;
}

// Main thread
QVariant Renderer::executeCommand(const QStringList &args)
{
#ifdef QT3D_JOBS_RUN_STATS
    return m_commandExecuter->executeCommand(args);
#else
    Q_UNUSED(args);
#endif
    return QVariant();
}

/*!
    \internal
    Called in the context of the aspect thread from QRenderAspect::onRegistered
*/
void Renderer::setOffscreenSurfaceHelper(OffscreenSurfaceHelper *helper)
{
    QMutexLocker locker(&m_offscreenSurfaceMutex);
    m_offscreenHelper = helper;
}

QSurfaceFormat Renderer::format()
{
    return m_format;
}

// When this function is called, we must not be processing the commands for frame n+1
void Renderer::prepareCommandsSubmission(const QVector<RenderView *> &renderViews)
{
    OpenGLVertexArrayObject *vao = nullptr;
    QHash<HVao, bool> updatedTable;

    for (RenderView *rv: renderViews) {
        const QVector<RenderCommand *> commands = rv->commands();
        for (RenderCommand *command : commands) {
            // Update/Create VAO
            if (command->m_type == RenderCommand::Draw) {
                Geometry *rGeometry = m_nodesManager->data<Geometry, GeometryManager>(command->m_geometry);
                GeometryRenderer *rGeometryRenderer = m_nodesManager->data<GeometryRenderer, GeometryRendererManager>(command->m_geometryRenderer);
                Shader *shader = m_nodesManager->data<Shader, ShaderManager>(command->m_shader);

                // We should never have inserted a command for which these are null
                // in the first place
                Q_ASSERT(rGeometry && rGeometryRenderer && shader);

                // The VAO should be created only once for a QGeometry and a ShaderProgram
                // Manager should have a VAO Manager that are indexed by QMeshData and Shader
                // RenderCommand should have a handle to the corresponding VAO for the Mesh and Shader
                HVao vaoHandle;

                // Create VAO or return already created instance associated with command shader/geometry
                // (VAO is emulated if not supported)
                createOrUpdateVAO(command, &vaoHandle, &vao);
                command->m_vao = vaoHandle;

                // Avoids redoing the same thing for the same VAO
                if (!updatedTable.contains(vaoHandle)) {
                    updatedTable.insert(vaoHandle, true);

                    // Do we have any attributes that are dirty ?
                    const bool requiresPartialVAOUpdate = requiresVAOAttributeUpdate(rGeometry, command);

                    // If true, we need to reupload all attributes to set the VAO
                    // Otherwise only dirty attributes will be updates
                    const bool requiresFullVAOUpdate = (!vao->isSpecified()) || (rGeometry->isDirty() || rGeometryRenderer->isDirty());

                    // Append dirty Geometry to temporary vector
                    // so that its dirtiness can be unset later
                    if (rGeometry->isDirty())
                        m_dirtyGeometry.push_back(rGeometry);

                    if (!command->m_attributes.isEmpty() && (requiresFullVAOUpdate || requiresPartialVAOUpdate)) {
                        // Activate shader
                        m_graphicsContext->activateShader(shader->dna());
                        // Bind VAO
                        vao->bind();
                        // Update or set Attributes and Buffers for the given rGeometry and Command
                        // Note: this fills m_dirtyAttributes as well
                        updateVAOWithAttributes(rGeometry, command, shader, requiresFullVAOUpdate);
                        vao->setSpecified(true);
                    }
                }

                // Unset dirtiness on rGeometryRenderer only
                // The rGeometry may be shared by several rGeometryRenderer
                // so we cannot unset its dirtiness at this point
                if (rGeometryRenderer->isDirty())
                    rGeometryRenderer->unsetDirty();

                // Prepare the ShaderParameterPack based on the active uniforms of the shader
                shader->prepareUniforms(command->m_parameterPack);

            } else if (command->m_type == RenderCommand::Compute) {
                Shader *shader = m_nodesManager->data<Shader, ShaderManager>(command->m_shader);
                Q_ASSERT(shader);

                // Prepare the ShaderParameterPack based on the active uniforms of the shader
                shader->prepareUniforms(command->m_parameterPack);
            }
        }
    }

    // Make sure we leave nothing bound
    if (vao)
        vao->release();

    // Unset dirtiness on Geometry and Attributes
    // Note: we cannot do it in the loop above as we want to be sure that all
    // the VAO which reference the geometry/attributes are properly updated
    for (Attribute *attribute : qAsConst(m_dirtyAttributes))
        attribute->unsetDirty();
    m_dirtyAttributes.clear();

    for (Geometry *geometry : qAsConst(m_dirtyGeometry))
        geometry->unsetDirty();
    m_dirtyGeometry.clear();
}

// Executed in a job
void Renderer::lookForDirtyBuffers()
{
    const QVector<HBuffer> activeBufferHandles = m_nodesManager->bufferManager()->activeHandles();
    for (HBuffer handle: activeBufferHandles) {
        Buffer *buffer = m_nodesManager->bufferManager()->data(handle);
        if (buffer->isDirty())
            m_dirtyBuffers.push_back(handle);
    }
}

// Executed in a job
void Renderer::lookForDirtyTextures()
{
    const QVector<HTexture> activeTextureHandles = m_nodesManager->textureManager()->activeHandles();
    for (HTexture handle: activeTextureHandles) {
        Texture *texture = m_nodesManager->textureManager()->data(handle);
        // Dirty meaning that something has changed on the texture
        // either properties, parameters, generator or a texture image
        if (texture->dirtyFlags() != Texture::NotDirty)
            m_dirtyTextures.push_back(handle);
    }
}

// Executed in a job
void Renderer::lookForDirtyShaders()
{
    if (isRunning()) {
        const QVector<HTechnique> activeTechniques = m_nodesManager->techniqueManager()->activeHandles();
        for (HTechnique techniqueHandle : activeTechniques) {
            Technique *technique = m_nodesManager->techniqueManager()->data(techniqueHandle);
            // If api of the renderer matches the one from the technique
            if (*contextInfo() == *technique->graphicsApiFilter()) {
                const auto passIds = technique->renderPasses();
                for (const QNodeId passId : passIds) {
                    RenderPass *renderPass = m_nodesManager->renderPassManager()->lookupResource(passId);
                    HShader shaderHandle = m_nodesManager->shaderManager()->lookupHandle(renderPass->shaderProgram());
                    Shader *shader = m_nodesManager->shaderManager()->data(shaderHandle);
                    if (shader != nullptr && !shader->isLoaded())
                        m_dirtyShaders.push_back(shaderHandle);
                }
            }
        }
    }
}

// Render Thread
void Renderer::updateGLResources()
{
    const QVector<HBuffer> dirtyBufferHandles = std::move(m_dirtyBuffers);
    for (HBuffer handle: dirtyBufferHandles) {
        Buffer *buffer = m_nodesManager->bufferManager()->data(handle);
        // Perform data upload
        // Forces creation if it doesn't exit
        if (!m_graphicsContext->hasGLBufferForBuffer(buffer))
            m_graphicsContext->glBufferForRenderBuffer(buffer);
        else // Otherwise update the glBuffer
            m_graphicsContext->updateBuffer(buffer);
        buffer->unsetDirty();
    }

    const QVector<HShader> dirtyShaderHandles = std::move(m_dirtyShaders);
    for (HShader handle: dirtyShaderHandles) {
        Shader *shader = m_nodesManager->shaderManager()->data(handle);
        // Compile shader
        m_graphicsContext->loadShader(shader);
    }

    const QVector<HTexture> activeTextureHandles = std::move(m_dirtyTextures);
    for (HTexture handle: activeTextureHandles) {
        Texture *texture = m_nodesManager->textureManager()->data(handle);
        // Upload/Update texture
        updateTexture(texture);
    }
    // When Textures are cleaned up, their id is saved
    // so that they can be cleaned up in the render thread
    // Note: we perform this step in second so that the previous updateTexture call
    // has a chance to find a shared texture
    const QVector<Qt3DCore::QNodeId> cleanedUpTextureIds = m_nodesManager->textureManager()->takeTexturesIdsToCleanup();
    for (const Qt3DCore::QNodeId textureCleanedUpId: cleanedUpTextureIds) {
        cleanupTexture(m_nodesManager->textureManager()->lookupResource(textureCleanedUpId));
        // We can really release the texture at this point
        m_nodesManager->textureManager()->releaseResource(textureCleanedUpId);
    }


}

// Render Thread
void Renderer::updateTexture(Texture *texture)
{
    // For implementing unique, non-shared, non-cached textures.
    // for now, every texture is shared by default

    bool isUnique = false;

    // TO DO: Update the vector once per frame (or in a job)
    const QVector<HAttachment> activeRenderTargetOutputs = m_nodesManager->attachmentManager()->activeHandles();
    // A texture is unique if it's being reference by a render target output
    for (const HAttachment attachmentHandle : activeRenderTargetOutputs) {
        RenderTargetOutput *attachment = m_nodesManager->attachmentManager()->data(attachmentHandle);
        if (attachment->textureUuid() == texture->peerId()) {
            isUnique = true;
            break;
        }
    }

    // Try to find the associated GLTexture for the backend Texture
    GLTextureManager *glTextureManager = m_nodesManager->glTextureManager();
    GLTexture *glTexture = glTextureManager->lookupResource(texture->peerId());

    // No GLTexture associated yet -> create it
    if (glTexture == nullptr) {
        if (isUnique)
            glTextureManager->createUnique(texture);
        else
            glTextureManager->getOrCreateShared(texture);
        texture->unsetDirty();
        return;
    }

    // if this texture is a shared texture, we might need to look for a new TextureImpl
    // and abandon the old one
    if (glTextureManager->isShared(glTexture)) {
        glTextureManager->abandon(glTexture, texture);
        // Check if a shared texture should become unique
        if (isUnique)
            glTextureManager->createUnique(texture);
        else
            glTextureManager->getOrCreateShared(texture);
        texture->unsetDirty();
        return;
    }

    // this texture node is the only one referring to the GLTexture.
    // we could thus directly modify the texture. Instead, for non-unique textures,
    // we first see if there is already a matching texture present.
    if (!isUnique) {
        GLTexture *newSharedTex = glTextureManager->findMatchingShared(texture);
        if (newSharedTex && newSharedTex != glTexture) {
            glTextureManager->abandon(glTexture, texture);
            glTextureManager->adoptShared(newSharedTex, texture);
            texture->unsetDirty();
            return;
        }
    }

    // we hold a reference to a unique or exclusive access to a shared texture
    // we can thus modify the texture directly.
    const Texture::DirtyFlags dirtyFlags = texture->dirtyFlags();

    if (dirtyFlags.testFlag(Texture::DirtyProperties) &&
            !glTextureManager->setProperties(glTexture, texture->properties()))
        qWarning() << "[Qt3DRender::TextureNode] updateTexture: TextureImpl.setProperties failed, should be non-shared";

    if (dirtyFlags.testFlag(Texture::DirtyParameters) &&
            !glTextureManager->setParameters(glTexture, texture->parameters()))
        qWarning() << "[Qt3DRender::TextureNode] updateTexture: TextureImpl.setParameters failed, should be non-shared";

    if (dirtyFlags.testFlag(Texture::DirtyGenerators) &&
            !glTextureManager->setImages(glTexture, texture->textureImages()))
        qWarning() << "[Qt3DRender::TextureNode] updateTexture: TextureImpl.setGenerators failed, should be non-shared";

    // Unset the dirty flag on the texture
    texture->unsetDirty();
}

// Render Thread
void Renderer::cleanupTexture(const Texture *texture)
{
    GLTextureManager *glTextureManager = m_nodesManager->glTextureManager();
    GLTexture *glTexture = glTextureManager->lookupResource(texture->peerId());

    if (glTexture != nullptr)
        glTextureManager->abandon(glTexture, texture);
}


// Happens in RenderThread context when all RenderViewJobs are done
// Returns the id of the last bound FBO
Renderer::ViewSubmissionResultData Renderer::submitRenderViews(const QVector<Render::RenderView *> &renderViews)
{
    QElapsedTimer timer;
    quint64 queueElapsed = 0;
    timer.start();

    const int renderViewsCount = renderViews.size();
    quint64 frameElapsed = queueElapsed;
    m_lastFrameCorrect.store(1);    // everything fine until now.....

    qCDebug(Memory) << Q_FUNC_INFO << "rendering frame ";

    // We might not want to render on the default FBO
    uint lastBoundFBOId = m_graphicsContext->boundFrameBufferObject();
    QSurface *surface = nullptr;
    QSurface *previousSurface = nullptr;
    for (const Render::RenderView *rv: renderViews) {
        previousSurface = rv->surface();
        if (previousSurface)
            break;
    }
    QSurface *lastUsedSurface = nullptr;

    for (int i = 0; i < renderViewsCount; ++i) {
        // Initialize GraphicsContext for drawing
        // If the RenderView has a RenderStateSet defined
        const RenderView *renderView = renderViews.at(i);

        // Check if using the same surface as the previous RenderView.
        // If not, we have to free up the context from the previous surface
        // and make the context current on the new surface
        surface = renderView->surface();
        SurfaceLocker surfaceLock(surface);

        // TO DO: Make sure that the surface we are rendering too has not been unset

        // For now, if we do not have a surface, skip this renderview
        // TODO: Investigate if it's worth providing a fallback offscreen surface
        //       to use when surface is null. Or if we should instead expose an
        //       offscreensurface to Qt3D.
        if (!surface || !surfaceLock.isSurfaceValid()) {
            m_lastFrameCorrect.store(0);
            continue;
        }

        lastUsedSurface = surface;
        const bool surfaceHasChanged = surface != previousSurface;

        if (surfaceHasChanged && previousSurface) {
            const bool swapBuffers = (lastBoundFBOId == m_graphicsContext->defaultFBO()) && PlatformSurfaceFilter::isSurfaceValid(previousSurface);
            // We only call swap buffer if we are sure the previous surface is still valid
            m_graphicsContext->endDrawing(swapBuffers);
        }

        if (surfaceHasChanged) {
            // If we can't make the context current on the surface, skip to the
            // next RenderView. We won't get the full frame but we may get something
            if (!m_graphicsContext->beginDrawing(surface)) {
                qWarning() << "Failed to make OpenGL context current on surface";
                m_lastFrameCorrect.store(0);
                continue;
            }

            previousSurface = surface;
            lastBoundFBOId = m_graphicsContext->boundFrameBufferObject();
        }

        // Note: the RenderStateSet is allocated once per RV if needed
        // and it contains a list of StateVariant value types
        RenderStateSet *renderViewStateSet = renderView->stateSet();

        // Set the RV state if not null,
        if (renderViewStateSet != nullptr)
            m_graphicsContext->setCurrentStateSet(renderViewStateSet);
        else
            m_graphicsContext->setCurrentStateSet(m_defaultRenderStateSet);

        // Set RenderTarget ...
        // Activate RenderTarget
        m_graphicsContext->activateRenderTarget(renderView->renderTargetId(),
                                                renderView->attachmentPack(),
                                                lastBoundFBOId);

        // set color, depth, stencil clear values (only if needed)
        auto clearBufferTypes = renderView->clearTypes();
        if (clearBufferTypes & QClearBuffers::ColorBuffer) {
            const QVector4D vCol = renderView->globalClearColorBufferInfo().clearColor;
            m_graphicsContext->clearColor(QColor::fromRgbF(vCol.x(), vCol.y(), vCol.z(), vCol.w()));
        }
        if (clearBufferTypes & QClearBuffers::DepthBuffer)
            m_graphicsContext->clearDepthValue(renderView->clearDepthValue());
        if (clearBufferTypes & QClearBuffers::StencilBuffer)
            m_graphicsContext->clearStencilValue(renderView->clearStencilValue());

        // Clear BackBuffer
        m_graphicsContext->clearBackBuffer(clearBufferTypes);

        // if there are ClearColors set for different draw buffers,
        // clear each of these draw buffers individually now
        const QVector<ClearBufferInfo> clearDrawBuffers = renderView->specificClearColorBufferInfo();
        for (const ClearBufferInfo &clearBuffer : clearDrawBuffers)
            m_graphicsContext->clearBufferf(clearBuffer.drawBufferIndex, clearBuffer.clearColor);

        // Set the Viewport
        m_graphicsContext->setViewport(renderView->viewport(), renderView->surfaceSize() * renderView->devicePixelRatio());

        // Execute the render commands
        if (!executeCommandsSubmission(renderView))
            m_lastFrameCorrect.store(0);    // something went wrong; make sure to render the next frame!

        // executeCommands takes care of restoring the stateset to the value
        // of gc->currentContext() at the moment it was called (either
        // renderViewStateSet or m_defaultRenderStateSet)
        if (!renderView->renderCaptureNodeId().isNull()) {
            QSize size = m_graphicsContext->renderTargetSize(renderView->surfaceSize() * renderView->devicePixelRatio());
            QImage image = m_graphicsContext->readFramebuffer(size);
            Render::RenderCapture *renderCapture =
                    static_cast<Render::RenderCapture*>(m_nodesManager->frameGraphManager()->lookupNode(renderView->renderCaptureNodeId()));
            renderCapture->addRenderCapture(image);
            addRenderCaptureSendRequest(renderView->renderCaptureNodeId());
        }

        frameElapsed = timer.elapsed() - frameElapsed;
        qCDebug(Rendering) << Q_FUNC_INFO << "Submitted Renderview " << i + 1 << "/" << renderViewsCount  << "in " << frameElapsed << "ms";
        frameElapsed = timer.elapsed();
    }

    // Bind lastBoundFBOId back. Needed also in threaded mode.
    // lastBoundFBOId != m_graphicsContext->activeFBO() when the last FrameGraph leaf node/renderView
    // contains RenderTargetSelector/RenderTarget
    if (lastBoundFBOId != m_graphicsContext->activeFBO())
        m_graphicsContext->bindFramebuffer(lastBoundFBOId);

    // Reset state and call doneCurrent if the surface
    // is valid and was actually activated
    if (surface && m_graphicsContext->hasValidGLHelper()) {
        // Reset state to the default state if the last stateset is not the
        // defaultRenderStateSet
        if (m_graphicsContext->currentStateSet() != m_defaultRenderStateSet)
            m_graphicsContext->setCurrentStateSet(m_defaultRenderStateSet);
    }

    queueElapsed = timer.elapsed() - queueElapsed;
    qCDebug(Rendering) << Q_FUNC_INFO << "Submission of Queue in " << queueElapsed << "ms <=> " << queueElapsed / renderViewsCount << "ms per RenderView <=> Avg " << 1000.0f / (queueElapsed * 1.0f/ renderViewsCount * 1.0f) << " RenderView/s";
    qCDebug(Rendering) << Q_FUNC_INFO << "Submission Completed in " << timer.elapsed() << "ms";

    // Stores the necessary information to safely perform
    // the last swap buffer call
    ViewSubmissionResultData resultData;
    resultData.lastBoundFBOId = lastBoundFBOId;
    resultData.surface = lastUsedSurface;

    return resultData;
}

void Renderer::markDirty(BackendNodeDirtySet changes, BackendNode *node)
{
    Q_UNUSED(node);
    m_changeSet |= changes;
}

Renderer::BackendNodeDirtySet Renderer::dirtyBits()
{
    return m_changeSet;
}

void Renderer::clearDirtyBits(BackendNodeDirtySet changes)
{
    m_changeSet &= ~changes;
}

bool Renderer::shouldRender()
{
    // Only render if something changed during the last frame, or the last frame
    // was not rendered successfully (or render-on-demand is disabled)
    return (m_settings->renderPolicy() == QRenderSettings::Always
            || m_changeSet != 0
            || !m_lastFrameCorrect.load());
}

void Renderer::skipNextFrame()
{
    Q_ASSERT(m_settings->renderPolicy() != QRenderSettings::Always);

    // make submitRenderViews() actually run
    m_renderQueue->setNoRender();
    m_submitRenderViewsSemaphore.release(1);
}

// Waits to be told to create jobs for the next frame
// Called by QRenderAspect jobsToExecute context of QAspectThread
// Returns all the jobs (and with proper dependency chain) required
// for the rendering of the scene
QVector<Qt3DCore::QAspectJobPtr> Renderer::renderBinJobs()
{
    QVector<QAspectJobPtr> renderBinJobs;

    // Create the jobs to build the frame
    const QVector<QAspectJobPtr> bufferJobs = createRenderBufferJobs();

    // Remove previous dependencies
    m_calculateBoundingVolumeJob->removeDependency(QWeakPointer<QAspectJob>());
    m_cleanupJob->removeDependency(QWeakPointer<QAspectJob>());

    // Set dependencies
    for (const QAspectJobPtr &bufferJob : bufferJobs)
        m_calculateBoundingVolumeJob->addDependency(bufferJob);

    // Set values on pickBoundingVolumeJob
    m_pickBoundingVolumeJob->setFrameGraphRoot(frameGraphRoot());
    m_pickBoundingVolumeJob->setRenderSettings(settings());
    m_pickBoundingVolumeJob->setMouseEvents(pendingPickingEvents());


    // Traverse the current framegraph. For each leaf node create a
    // RenderView and set its configuration then create a job to
    // populate the RenderView with a set of RenderCommands that get
    // their details from the RenderNodes that are visible to the
    // Camera selected by the framegraph configuration
    FrameGraphVisitor visitor(this, m_nodesManager->frameGraphManager());
    visitor.traverse(frameGraphRoot(), &renderBinJobs);

    // Set dependencies of resource gatherer
    for (const QAspectJobPtr &jobPtr : renderBinJobs) {
        jobPtr->addDependency(m_bufferGathererJob);
        jobPtr->addDependency(m_textureGathererJob);
        jobPtr->addDependency(m_shaderGathererJob);
    }

    // Add jobs
    renderBinJobs.push_back(m_updateShaderDataTransformJob);
    renderBinJobs.push_back(m_updateMeshTriangleListJob);
    renderBinJobs.push_back(m_expandBoundingVolumeJob);
    renderBinJobs.push_back(m_updateWorldBoundingVolumeJob);
    renderBinJobs.push_back(m_calculateBoundingVolumeJob);
    renderBinJobs.push_back(m_worldTransformJob);
    renderBinJobs.push_back(m_cleanupJob);
    renderBinJobs.push_back(m_sendRenderCaptureJob);
    renderBinJobs.append(bufferJobs);

    // Jobs to prepare GL Resource upload
    renderBinJobs.push_back(m_syncTextureLoadingJob);
    renderBinJobs.push_back(m_bufferGathererJob);
    renderBinJobs.push_back(m_textureGathererJob);
    renderBinJobs.push_back(m_shaderGathererJob);

    // Set target number of RenderViews
    m_renderQueue->setTargetRenderViewCount(visitor.leafNodeCount());

    return renderBinJobs;
}

QAspectJobPtr Renderer::pickBoundingVolumeJob()
{
    return m_pickBoundingVolumeJob;
}

QAspectJobPtr Renderer::syncTextureLoadingJob()
{
    return m_syncTextureLoadingJob;
}

QAbstractFrameAdvanceService *Renderer::frameAdvanceService() const
{
    return static_cast<Qt3DCore::QAbstractFrameAdvanceService *>(m_vsyncFrameAdvanceService.data());
}

// Called by executeCommands
void Renderer::performDraw(RenderCommand *command)
{
    if (command->m_primitiveType == QGeometryRenderer::Patches)
        m_graphicsContext->setVerticesPerPatch(command->m_verticesPerPatch);

    if (command->m_primitiveRestartEnabled)
        m_graphicsContext->enablePrimitiveRestart(command->m_restartIndexValue);

    // TO DO: Add glMulti Draw variants
    if (command->m_drawIndexed) {
        m_graphicsContext->drawElementsInstancedBaseVertexBaseInstance(command->m_primitiveType,
                                                                       command->m_primitiveCount,
                                                                       command->m_indexAttributeDataType,
                                                                       reinterpret_cast<void*>(quintptr(command->m_indexAttributeByteOffset)),
                                                                       command->m_instanceCount,
                                                                       command->m_indexOffset,
                                                                       command->m_firstVertex);
    } else {
        m_graphicsContext->drawArraysInstancedBaseInstance(command->m_primitiveType,
                                                           command->m_firstInstance,
                                                           command->m_primitiveCount,
                                                           command->m_instanceCount,
                                                           command->m_firstVertex);
    }

#if defined(QT3D_RENDER_ASPECT_OPENGL_DEBUG)
    int err = m_graphicsContext->openGLContext()->functions()->glGetError();
    if (err)
        qCWarning(Rendering) << "GL error after drawing mesh:" << QString::number(err, 16);
#endif

    if (command->m_primitiveRestartEnabled)
        m_graphicsContext->disablePrimitiveRestart();
}

void Renderer::performCompute(const RenderView *, RenderCommand *command)
{
    m_graphicsContext->activateShader(command->m_shaderDna);
    m_graphicsContext->setParameters(command->m_parameterPack);
    m_graphicsContext->dispatchCompute(command->m_workGroups[0],
            command->m_workGroups[1],
            command->m_workGroups[2]);

    // HACK: Reset the compute flag to dirty
    m_changeSet |= AbstractRenderer::ComputeDirty;

#if defined(QT3D_RENDER_ASPECT_OPENGL_DEBUG)
    int err = m_graphicsContext->openGLContext()->functions()->glGetError();
    if (err)
        qCWarning(Rendering) << "GL error after drawing mesh:" << QString::number(err, 16);
#endif
}

void Renderer::createOrUpdateVAO(RenderCommand *command,
                                 HVao *previousVaoHandle,
                                 OpenGLVertexArrayObject **vao)
{
    VAOManager *vaoManager = m_nodesManager->vaoManager();
    command->m_vao = vaoManager->lookupHandle(QPair<HGeometry, HShader>(command->m_geometry, command->m_shader));

    if (command->m_vao.isNull()) {
        qCDebug(Rendering) << Q_FUNC_INFO << "Allocating new VAO";
        command->m_vao = vaoManager->getOrAcquireHandle(QPair<HGeometry, HShader>(command->m_geometry, command->m_shader));
        vaoManager->data(command->m_vao)->setGraphicsContext(m_graphicsContext.data());
        if (m_graphicsContext->supportsVAO())
            vaoManager->data(command->m_vao)->setVao(new QOpenGLVertexArrayObject());
        vaoManager->data(command->m_vao)->create();
    }

    if (*previousVaoHandle != command->m_vao) {
        *previousVaoHandle = command->m_vao;
        *vao = vaoManager->data(command->m_vao);
    }
    Q_ASSERT(*vao);
}

// Called by RenderView->submit() in RenderThread context
// Returns true, if all RenderCommands were sent to the GPU
bool Renderer::executeCommandsSubmission(const RenderView *rv)
{
    bool allCommandsIssued = true;

    // Render drawing commands
    const QVector<RenderCommand *> commands = rv->commands();

    // Use the graphicscontext to submit the commands to the underlying
    // graphics API (OpenGL)

    // Save the RenderView base stateset
    RenderStateSet *globalState = m_graphicsContext->currentStateSet();
    OpenGLVertexArrayObject *vao = nullptr;

    for (RenderCommand *command : qAsConst(commands)) {

        if (command->m_type == RenderCommand::Compute) { // Compute Call
            performCompute(rv, command);
        } else { // Draw Command

            // Check if we have a valid command that can be drawn
            if (!command->m_isValid) {
                allCommandsIssued = false;
                continue;
            }

            vao = m_nodesManager->vaoManager()->data(command->m_vao);

            //// We activate the shader here
            m_graphicsContext->activateShader(command->m_shaderDna);

            // Bind VAO
            vao->bind();

            //// Update program uniforms
            m_graphicsContext->setParameters(command->m_parameterPack);

            //// OpenGL State
            // TO DO: Make states not dependendent on their backend node for this step
            // Set state
            RenderStateSet *localState = command->m_stateSet;
            // Merge the RenderCommand state with the globalState of the RenderView
            // Or restore the globalState if no stateSet for the RenderCommand
            if (localState != nullptr) {
                command->m_stateSet->merge(globalState);
                m_graphicsContext->setCurrentStateSet(command->m_stateSet);
            } else {
                m_graphicsContext->setCurrentStateSet(globalState);
            }
            // All Uniforms for a pass are stored in the QUniformPack of the command
            // Uniforms for Effect, Material and Technique should already have been correctly resolved
            // at that point

            //// Draw Calls
            performDraw(command);
        }
    } // end of RenderCommands loop

    // We cache the VAO and release it only at the end of the exectute frame
    // We try to minimize VAO binding between RenderCommands
    if (vao)
        vao->release();

    // Reset to the state we were in before executing the render commands
    m_graphicsContext->setCurrentStateSet(globalState);

    return allCommandsIssued;
}

void Renderer::updateVAOWithAttributes(Geometry *geometry,
                                       RenderCommand *command,
                                       Shader *shader,
                                       bool forceUpdate)
{
    m_dirtyAttributes.reserve(m_dirtyAttributes.size() + geometry->attributes().size());
    const auto attributeIds = geometry->attributes();
    for (QNodeId attributeId : attributeIds) {
        // TO DO: Improvement we could store handles and use the non locking policy on the attributeManager
        Attribute *attribute = m_nodesManager->attributeManager()->lookupResource(attributeId);

        if (attribute == nullptr)
            continue;

        Buffer *buffer = m_nodesManager->bufferManager()->lookupResource(attribute->bufferId());

        // Buffer update was already performed at this point
        // Just make sure the attribute reference a valid buffer
        if (buffer == nullptr)
            continue;

        // Index Attribute
        bool attributeWasDirty = false;
        if (attribute->attributeType() == QAttribute::IndexAttribute) {
            if ((attributeWasDirty = attribute->isDirty()) == true || forceUpdate)
                m_graphicsContext->specifyIndices(buffer);
            // Vertex Attribute
        } else if (command->m_attributes.contains(attribute->nameId())) {
            if ((attributeWasDirty = attribute->isDirty()) == true || forceUpdate) {
                // Find the location for the attribute
                const QVector<ShaderAttribute> shaderAttributes = shader->attributes();
                int attributeLocation = -1;
                for (const ShaderAttribute &shaderAttribute : shaderAttributes) {
                    if (shaderAttribute.m_nameId == attribute->nameId()) {
                        attributeLocation = shaderAttribute.m_location;
                        break;
                    }
                }
                m_graphicsContext->specifyAttribute(attribute, buffer, attributeLocation);
            }
        }

        // Append attribute to temporary vector so that its dirtiness
        // can be cleared at the end of the frame
        if (attributeWasDirty)
            m_dirtyAttributes.push_back(attribute);

        // Note: We cannot call unsertDirty on the Attribute at this
        // point as we don't know if the attributes are being shared
        // with other geometry / geometryRenderer in which case they still
        // should remain dirty so that VAO for these commands are properly
        // updated
    }
}

bool Renderer::requiresVAOAttributeUpdate(Geometry *geometry,
                                          RenderCommand *command) const
{
    const auto attributeIds = geometry->attributes();

    for (QNodeId attributeId : attributeIds) {
        // TO DO: Improvement we could store handles and use the non locking policy on the attributeManager
        Attribute *attribute = m_nodesManager->attributeManager()->lookupResource(attributeId);

        if (attribute == nullptr)
            continue;

        if ((attribute->attributeType() == QAttribute::IndexAttribute && attribute->isDirty()) ||
                (command->m_attributes.contains(attribute->nameId()) && attribute->isDirty()))
            return true;
    }
    return false;
}

// Erase graphics related resources that may become unused after a frame
void Renderer::cleanGraphicsResources()
{
    // Clean buffers
    const QVector<Qt3DCore::QNodeId> buffersToRelease = m_nodesManager->bufferManager()->takeBuffersToRelease();
    for (Qt3DCore::QNodeId bufferId : buffersToRelease)
        m_graphicsContext->releaseBuffer(bufferId);

    // Delete abandoned textures
    const QVector<GLTexture*> abandonedTextures = m_nodesManager->glTextureManager()->takeAbandonedTextures();
    for (GLTexture *tex : abandonedTextures) {
        tex->destroyGLTexture();
        delete tex;
    }
}

QList<QMouseEvent> Renderer::pendingPickingEvents() const
{
    return m_pickEventFilter->pendingEvents();
}

const GraphicsApiFilterData *Renderer::contextInfo() const
{
    return m_graphicsContext->contextInfo();
}

GraphicsContext *Renderer::graphicsContext() const
{
    return m_graphicsContext.data();
}

void Renderer::addRenderCaptureSendRequest(Qt3DCore::QNodeId nodeId)
{
    if (!m_pendingRenderCaptureSendRequests.contains(nodeId))
        m_pendingRenderCaptureSendRequests.push_back(nodeId);
}

const QVector<Qt3DCore::QNodeId> Renderer::takePendingRenderCaptureSendRequests()
{
    return std::move(m_pendingRenderCaptureSendRequests);
}

// Returns a vector of jobs to be performed for dirty buffers
// 1 dirty buffer == 1 job, all job can be performed in parallel
QVector<Qt3DCore::QAspectJobPtr> Renderer::createRenderBufferJobs() const
{
    const QVector<QNodeId> dirtyBuffers = m_nodesManager->bufferManager()->dirtyBuffers();
    QVector<QAspectJobPtr> dirtyBuffersJobs;
    dirtyBuffersJobs.reserve(dirtyBuffers.size());

    for (const QNodeId bufId : dirtyBuffers) {
        Render::HBuffer bufferHandle = m_nodesManager->lookupHandle<Render::Buffer, Render::BufferManager, Render::HBuffer>(bufId);
        if (!bufferHandle.isNull()) {
            // Create new buffer job
            auto job = Render::LoadBufferJobPtr::create(bufferHandle);
            job->setNodeManager(m_nodesManager);
            dirtyBuffersJobs.push_back(job);
        }
    }

    return dirtyBuffersJobs;
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
