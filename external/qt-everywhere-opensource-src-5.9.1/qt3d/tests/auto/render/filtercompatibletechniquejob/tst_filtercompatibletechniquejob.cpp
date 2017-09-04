/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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


#include <QtTest/QTest>
#include <Qt3DCore/private/qaspectjobmanager_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qviewport.h>
#include <Qt3DRender/private/technique_p.h>
#include <Qt3DRender/private/filtercompatibletechniquejob_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/graphicscontext_p.h>
#include <Qt3DRender/private/qrenderaspect_p.h>
#include <Qt3DRender/private/techniquemanager_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

class TestAspect : public Qt3DRender::QRenderAspect
{
public:
    TestAspect(Qt3DCore::QNode *root)
        : Qt3DRender::QRenderAspect(Qt3DRender::QRenderAspect::Synchronous)
        , m_jobManager(new Qt3DCore::QAspectJobManager())
        , m_window(new QWindow())
    {
        m_window->setSurfaceType(QWindow::OpenGLSurface);
        m_window->setGeometry(0, 0, 10, 10);
        m_window->create();

        if (!m_glContext.create()) {
            qWarning() << "Failed to create OpenGL context";
            return;
        }

        if (!m_glContext.makeCurrent(m_window.data())) {
            qWarning() << "Failed to make OpenGL context current";
            return;
        }

        Qt3DCore::QAbstractAspectPrivate::get(this)->m_jobManager = m_jobManager.data();
        QRenderAspect::onRegistered();

        const Qt3DCore::QNodeCreatedChangeGenerator generator(root);
        const QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = generator.creationChanges();

        for (const Qt3DCore::QNodeCreatedChangeBasePtr change : creationChanges)
            d_func()->createBackendNode(change);

    }

    ~TestAspect()
    {
        QRenderAspect::onUnregistered();
    }

    Qt3DRender::Render::NodeManagers *nodeManagers() const
    {
        return d_func()->m_renderer->nodeManagers();
    }

    void initializeRenderer()
    {
        renderer()->setOpenGLContext(&m_glContext);
        d_func()->m_renderer->initialize();
        renderer()->graphicsContext()->beginDrawing(m_window.data());
    }

    Render::Renderer *renderer() const
    {
        return static_cast<Render::Renderer *>(d_func()->m_renderer);
    }

    void onRegistered() { QRenderAspect::onRegistered(); }
    void onUnregistered() { QRenderAspect::onUnregistered(); }

private:
    QScopedPointer<Qt3DCore::QAspectJobManager> m_jobManager;
    QScopedPointer<QWindow> m_window;
    QOpenGLContext m_glContext;
};

} // namespace Qt3DRender

QT_END_NAMESPACE

namespace {

Qt3DCore::QEntity *buildTestScene()
{
    Qt3DCore::QEntity *root = new Qt3DCore::QEntity();

    // FrameGraph
    Qt3DRender::QRenderSettings* renderSettings = new Qt3DRender::QRenderSettings();
    renderSettings->setActiveFrameGraph(new Qt3DRender::QViewport());
    root->addComponent(renderSettings);

    // Scene
    Qt3DRender::QTechnique *gl2Technique = new Qt3DRender::QTechnique(root);
    gl2Technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    gl2Technique->graphicsApiFilter()->setMajorVersion(2);
    gl2Technique->graphicsApiFilter()->setMinorVersion(0);
    gl2Technique->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::NoProfile);

    Qt3DRender::QTechnique *gl3Technique = new Qt3DRender::QTechnique(root);
    gl3Technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    gl3Technique->graphicsApiFilter()->setMajorVersion(3);
    gl3Technique->graphicsApiFilter()->setMinorVersion(2);
    gl3Technique->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);

    Qt3DRender::QTechnique *es2Technique = new Qt3DRender::QTechnique(root);
    es2Technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGLES);
    es2Technique->graphicsApiFilter()->setMajorVersion(2);
    es2Technique->graphicsApiFilter()->setMinorVersion(0);
    es2Technique->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::NoProfile);

    return root;
}

} // anonymous

class tst_FilterCompatibleTechniqueJob : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        QSurfaceFormat format;
#ifdef QT_OPENGL_ES_2
        format.setRenderableType(QSurfaceFormat::OpenGLES);
#else
        if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
            format.setVersion(4, 3);
            format.setProfile(QSurfaceFormat::CoreProfile);
        }
#endif
        format.setDepthBufferSize(24);
        format.setSamples(4);
        format.setStencilBufferSize(8);
        QSurfaceFormat::setDefaultFormat(format);
    }

    void checkInitialState()
    {
        // GIVEN
        Qt3DRender::Render::FilterCompatibleTechniqueJob backendFilterCompatibleTechniqueJob;

        // THEN
        QVERIFY(backendFilterCompatibleTechniqueJob.manager() == nullptr);
        QVERIFY(backendFilterCompatibleTechniqueJob.renderer() == nullptr);

        // WHEN
        Qt3DRender::Render::TechniqueManager techniqueManager;
        Qt3DRender::Render::Renderer renderer(Qt3DRender::QRenderAspect::Synchronous);
        backendFilterCompatibleTechniqueJob.setManager(&techniqueManager);
        backendFilterCompatibleTechniqueJob.setRenderer(&renderer);

        // THEN
        QCOMPARE(backendFilterCompatibleTechniqueJob.manager(), &techniqueManager);
        QCOMPARE(backendFilterCompatibleTechniqueJob.renderer(), &renderer);
    }

    void checkRunRendererNotRunning()
    {
        // GIVEN
        Qt3DRender::Render::FilterCompatibleTechniqueJob backendFilterCompatibleTechniqueJob;
        Qt3DRender::TestAspect testAspect(buildTestScene());

        // WHEN
        backendFilterCompatibleTechniqueJob.setManager(testAspect.nodeManagers()->techniqueManager());
        backendFilterCompatibleTechniqueJob.setRenderer(testAspect.renderer());
        testAspect.initializeRenderer();
        testAspect.renderer()->shutdown();

        // THEN
        QCOMPARE(testAspect.renderer()->isRunning(), false);
        QVector<Qt3DRender::Render::HTechnique> handles = testAspect.nodeManagers()->techniqueManager()->activeHandles();
        QCOMPARE(handles.size(), 3);

        // WHEN
        backendFilterCompatibleTechniqueJob.run();

        // THEN -> untouched since not running
        const QVector<Qt3DCore::QNodeId> dirtyTechniquesId = testAspect.nodeManagers()->techniqueManager()->takeDirtyTechniques();
        QCOMPARE(dirtyTechniquesId.size(), 3);
    }

    void checkRunRendererRunning()
    {
        // GIVEN
        Qt3DRender::Render::FilterCompatibleTechniqueJob backendFilterCompatibleTechniqueJob;
        Qt3DRender::TestAspect testAspect(buildTestScene());

        // WHEN
        backendFilterCompatibleTechniqueJob.setManager(testAspect.nodeManagers()->techniqueManager());
        backendFilterCompatibleTechniqueJob.setRenderer(testAspect.renderer());
        testAspect.initializeRenderer();

        // THEN
        QCOMPARE(testAspect.renderer()->isRunning(), true);
        QCOMPARE(testAspect.renderer()->graphicsContext()->isInitialized(), true);
        const QVector<Qt3DRender::Render::HTechnique> handles = testAspect.nodeManagers()->techniqueManager()->activeHandles();
        QCOMPARE(handles.size(), 3);

        // WHEN
        backendFilterCompatibleTechniqueJob.run();

        // THEN -> empty if job ran properly
        const QVector<Qt3DCore::QNodeId> dirtyTechniquesId = testAspect.nodeManagers()->techniqueManager()->takeDirtyTechniques();
        QCOMPARE(dirtyTechniquesId.size(), 0);

        // Check at least one technique is valid
        bool foundValid = false;
        for (const auto handle: handles) {
            Qt3DRender::Render::Technique *technique = testAspect.nodeManagers()->techniqueManager()->data(handle);
            foundValid |= technique->isCompatibleWithRenderer();
        }
        QCOMPARE(foundValid, true);
    }
};

QTEST_MAIN(tst_FilterCompatibleTechniqueJob)

#include "tst_filtercompatibletechniquejob.moc"
