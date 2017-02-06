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
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qtransform.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/private/qaspectjobmanager_p.h>

#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/private/materialparametergathererjob_p.h>
#include <Qt3DRender/private/techniquefilternode_p.h>
#include <Qt3DRender/private/renderpassfilternode_p.h>
#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qtechniquefilter.h>
#include <Qt3DRender/qrenderpassfilter.h>
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qshaderprogram.h>
#include <Qt3DRender/qviewport.h>
#include <Qt3DRender/private/qrenderaspect_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

class TestAspect : public Qt3DRender::QRenderAspect
{
public:
    TestAspect(Qt3DCore::QNode *root)
        : Qt3DRender::QRenderAspect(Qt3DRender::QRenderAspect::Synchronous)
        , m_jobManager(new Qt3DCore::QAspectJobManager())
    {
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
        d_func()->m_renderer->initialize();
    }

    Render::MaterialParameterGathererJobPtr materialGathererJob() const
    {
        Render::MaterialParameterGathererJobPtr job = Render::MaterialParameterGathererJobPtr::create();
        job->setNodeManagers(nodeManagers());
        job->setRenderer(static_cast<Render::Renderer *>(d_func()->m_renderer));
        return job;
    }

    void onRegistered() { QRenderAspect::onRegistered(); }
    void onUnregistered() { QRenderAspect::onUnregistered(); }

private:
    QScopedPointer<Qt3DCore::QAspectJobManager> m_jobManager;
};

} // namespace Qt3DRender

QT_END_NAMESPACE

namespace {

class TestMaterial : public Qt3DRender::QMaterial
{
    Q_OBJECT
public:
    explicit TestMaterial(Qt3DCore::QNode *parent = nullptr)
        : Qt3DRender::QMaterial(parent)
        , m_effect(new Qt3DRender::QEffect())
        , m_gl3Technique(new Qt3DRender::QTechnique())
        , m_gl2Technique(new Qt3DRender::QTechnique())
        , m_es2Technique(new Qt3DRender::QTechnique())
        , m_gl3Pass(new Qt3DRender::QRenderPass())
        , m_gl2Pass(new Qt3DRender::QRenderPass())
        , m_es2Pass(new Qt3DRender::QRenderPass())
        , m_gl3Program(new Qt3DRender::QShaderProgram())
        , m_gl2es2Program(new Qt3DRender::QShaderProgram())
    {
        m_gl3Pass->setShaderProgram(m_gl3Program);
        m_gl2Pass->setShaderProgram(m_gl2es2Program);
        m_es2Pass->setShaderProgram(m_gl2es2Program);

        m_gl3Technique->addRenderPass(m_gl3Pass);
        m_gl2Technique->addRenderPass(m_gl2Pass);
        m_es2Technique->addRenderPass(m_es2Pass);

        m_effect->addTechnique(m_gl3Technique);
        m_effect->addTechnique(m_gl2Technique);
        m_effect->addTechnique(m_es2Technique);

        setEffect(m_effect);
    }

    Qt3DRender::QTechnique *gl3Technique() const { return m_gl3Technique; }
    Qt3DRender::QTechnique *gl2Technique() const { return m_gl2Technique; }
    Qt3DRender::QTechnique *es2Technique() const { return m_es2Technique; }

    Qt3DRender::QRenderPass *gl3Pass() const { return m_gl3Pass; }
    Qt3DRender::QRenderPass *gl2Pass() const { return m_gl2Pass; }
    Qt3DRender::QRenderPass *es2Pass() const { return m_es2Pass; }

    Qt3DRender::QShaderProgram *gl3Shader() const { return m_gl3Program; }
    Qt3DRender::QShaderProgram *gl2shader() const { return m_gl2es2Program; }
    Qt3DRender::QShaderProgram *es2Shader() const { return m_gl2es2Program; }

private:
    Qt3DRender::QEffect *m_effect;
    Qt3DRender::QTechnique *m_gl3Technique;
    Qt3DRender::QTechnique *m_gl2Technique;
    Qt3DRender::QTechnique *m_es2Technique;
    Qt3DRender::QRenderPass *m_gl3Pass;
    Qt3DRender::QRenderPass *m_gl2Pass;
    Qt3DRender::QRenderPass *m_es2Pass;
    Qt3DRender::QShaderProgram *m_gl3Program;
    Qt3DRender::QShaderProgram *m_gl2es2Program;
};

Qt3DRender::QViewport *viewportFrameGraph()
{
    Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport();
    return viewport;
}

Qt3DRender::QTechniqueFilter  *techniqueFilterFrameGraph()
{
    Qt3DRender::QTechniqueFilter *techniqueFilter = new Qt3DRender::QTechniqueFilter();
    return techniqueFilter;
}

Qt3DRender::QRenderPassFilter  *renderPassFilter()
{
    Qt3DRender::QRenderPassFilter *passFilter = new Qt3DRender::QRenderPassFilter();
    return passFilter;
}

Qt3DCore::QEntity *buildScene(Qt3DRender::QFrameGraphNode *frameGraph, Qt3DRender::QMaterial *material = nullptr)
{
    Qt3DCore::QEntity *root = new Qt3DCore::QEntity();

    // FrameGraph
    Qt3DRender::QRenderSettings* renderSettings = new Qt3DRender::QRenderSettings();
    renderSettings->setActiveFrameGraph(frameGraph);
    root->addComponent(renderSettings);

    // Scene
    for (int i = 0; i < 10; i++) {
        Qt3DCore::QEntity *e = new Qt3DCore::QEntity();
        if (material != nullptr)
            e->addComponent(material);
        e->setParent(root);
    }

    return root;
}

} // anonymous

class tst_MaterialParameterGatherer : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void checkRunNoHandlesNoTechniqueFilterNoPassFilter()
    {
        // GIVEN
        Qt3DCore::QEntity *sceneRoot = buildScene(viewportFrameGraph());
        Qt3DRender::TestAspect testAspect(sceneRoot);
        Qt3DRender::Render::MaterialParameterGathererJobPtr gatherer = testAspect.materialGathererJob();

        testAspect.initializeRenderer();

        // THEN
        QCOMPARE(testAspect.nodeManagers()->materialManager()->activeHandles().size(), 0);

        // WHEN
        gatherer->run();

        // THEN
        QCOMPARE(gatherer->materialToPassAndParameter().size(), 0);
    }

    void checkRunSelectAllNoTechniqueFilterNoPassFilter()
    {
        // GIVEN
        TestMaterial material;
        Qt3DCore::QEntity *sceneRoot = buildScene(viewportFrameGraph(), &material);
        Qt3DRender::TestAspect testAspect(sceneRoot);
        Qt3DRender::Render::MaterialParameterGathererJobPtr gatherer = testAspect.materialGathererJob();

        testAspect.initializeRenderer();

        // THEN
        QCOMPARE(testAspect.nodeManagers()->materialManager()->activeHandles().size(), 1);

        // WHEN
        gatherer->setHandles(testAspect.nodeManagers()->materialManager()->activeHandles());
        gatherer->run();

        // THEN
        QCOMPARE(gatherer->materialToPassAndParameter().size(), 1);
    }

    void checkRunDisabledMaterial()
    {
        // GIVEN
        TestMaterial material;
        material.setEnabled(false);
        Qt3DCore::QEntity *sceneRoot = buildScene(viewportFrameGraph(), &material);
        Qt3DRender::TestAspect testAspect(sceneRoot);
        Qt3DRender::Render::MaterialParameterGathererJobPtr gatherer = testAspect.materialGathererJob();

        testAspect.initializeRenderer();

        // THEN
        QCOMPARE(testAspect.nodeManagers()->materialManager()->activeHandles().size(), 1);

        // WHEN
        gatherer->setHandles(testAspect.nodeManagers()->materialManager()->activeHandles());
        gatherer->run();

        // THEN
        QCOMPARE(gatherer->materialToPassAndParameter().size(), 0);
    }

    void checkRunSelectAllTechniqueFilterWithNoFilterNoPassFilter()
    {
        // GIVEN
        Qt3DRender::QTechniqueFilter *frameGraphFilter = techniqueFilterFrameGraph();

        TestMaterial material;

        Qt3DRender::QFilterKey techniqueFilterKey;
        techniqueFilterKey.setName(QStringLiteral("renderingStyle"));
        techniqueFilterKey.setValue(QVariant(QStringLiteral("backward")));

        material.gl2Technique()->addFilterKey(&techniqueFilterKey);
        material.gl3Technique()->addFilterKey(&techniqueFilterKey);
        material.es2Technique()->addFilterKey(&techniqueFilterKey);

        Qt3DCore::QEntity *sceneRoot = buildScene(frameGraphFilter, &material);
        Qt3DRender::TestAspect testAspect(sceneRoot);
        Qt3DRender::Render::MaterialParameterGathererJobPtr gatherer = testAspect.materialGathererJob();

        testAspect.initializeRenderer();

        // THEN
        QCOMPARE(testAspect.nodeManagers()->materialManager()->activeHandles().size(), 1);
        Qt3DRender::Render::TechniqueFilter *backendTechniqueFilter = static_cast<Qt3DRender::Render::TechniqueFilter *>(testAspect.nodeManagers()->frameGraphManager()->lookupNode(frameGraphFilter->id()));
        QVERIFY(backendTechniqueFilter != nullptr);

        // WHEN
        gatherer->setHandles(testAspect.nodeManagers()->materialManager()->activeHandles());
        gatherer->setTechniqueFilter(backendTechniqueFilter);
        gatherer->run();

        // THEN
        QCOMPARE(gatherer->materialToPassAndParameter().size(), 1);
    }

    void checkRunSelectAllTechniqueFilterWithIncompatibleFilterNoPassFilter()
    {
        // GIVEN
        Qt3DRender::QTechniqueFilter *frameGraphFilter = techniqueFilterFrameGraph();
        TestMaterial material;

        Qt3DRender::QFilterKey techniqueFilterFilterKey;
        techniqueFilterFilterKey.setName(QStringLiteral("renderingStyle"));
        techniqueFilterFilterKey.setValue(QVariant(QStringLiteral("forward")));

        Qt3DRender::QFilterKey techniqueFilterKey;
        techniqueFilterKey.setName(QStringLiteral("renderingStyle"));
        techniqueFilterKey.setValue(QVariant(QStringLiteral("backward")));

        frameGraphFilter->addMatch(&techniqueFilterFilterKey);

        material.gl2Technique()->addFilterKey(&techniqueFilterKey);
        material.gl3Technique()->addFilterKey(&techniqueFilterKey);
        material.es2Technique()->addFilterKey(&techniqueFilterKey);

        Qt3DCore::QEntity *sceneRoot = buildScene(frameGraphFilter, &material);
        Qt3DRender::TestAspect testAspect(sceneRoot);
        Qt3DRender::Render::MaterialParameterGathererJobPtr gatherer = testAspect.materialGathererJob();

        testAspect.initializeRenderer();

        // THEN
        QCOMPARE(testAspect.nodeManagers()->materialManager()->activeHandles().size(), 1);
        Qt3DRender::Render::TechniqueFilter *backendTechniqueFilter = static_cast<Qt3DRender::Render::TechniqueFilter *>(testAspect.nodeManagers()->frameGraphManager()->lookupNode(frameGraphFilter->id()));
        QVERIFY(backendTechniqueFilter != nullptr);

        // WHEN
        gatherer->setHandles(testAspect.nodeManagers()->materialManager()->activeHandles());
        gatherer->setTechniqueFilter(backendTechniqueFilter);
        gatherer->run();

        // THEN
        QCOMPARE(gatherer->materialToPassAndParameter().size(), 0);
    }

    void checkRunSelectAllTechniqueFilterWithCompatibleFilterNoPassFilter()
    {
        // GIVEN
        Qt3DRender::QTechniqueFilter *frameGraphFilter = techniqueFilterFrameGraph();
        TestMaterial material;

        Qt3DRender::QFilterKey techniqueFilterKey;
        techniqueFilterKey.setName(QStringLiteral("renderingStyle"));
        techniqueFilterKey.setValue(QVariant(QStringLiteral("backward")));

        frameGraphFilter->addMatch(&techniqueFilterKey);

        material.gl2Technique()->addFilterKey(&techniqueFilterKey);
        material.gl3Technique()->addFilterKey(&techniqueFilterKey);
        material.es2Technique()->addFilterKey(&techniqueFilterKey);

        Qt3DCore::QEntity *sceneRoot = buildScene(frameGraphFilter, &material);
        Qt3DRender::TestAspect testAspect(sceneRoot);
        Qt3DRender::Render::MaterialParameterGathererJobPtr gatherer = testAspect.materialGathererJob();

        testAspect.initializeRenderer();

        // THEN
        QCOMPARE(testAspect.nodeManagers()->materialManager()->activeHandles().size(), 1);
        Qt3DRender::Render::TechniqueFilter *backendTechniqueFilter = static_cast<Qt3DRender::Render::TechniqueFilter *>(testAspect.nodeManagers()->frameGraphManager()->lookupNode(frameGraphFilter->id()));
        QVERIFY(backendTechniqueFilter != nullptr);

        // WHEN
        gatherer->setHandles(testAspect.nodeManagers()->materialManager()->activeHandles());
        gatherer->setTechniqueFilter(backendTechniqueFilter);
        gatherer->run();

        // THEN
        QCOMPARE(gatherer->materialToPassAndParameter().size(), 1);
    }

    void checkRunSelectAllNoTechniqueFilterPassFilterWithNoFilter()
    {
        // GIVEN
        Qt3DRender::QRenderPassFilter *frameGraphFilter = renderPassFilter();
        TestMaterial material;

        Qt3DRender::QFilterKey passFilterKey;
        passFilterKey.setName(QStringLiteral("renderingStyle"));
        passFilterKey.setValue(QVariant(QStringLiteral("backward")));

        material.gl3Pass()->addFilterKey(&passFilterKey);
        material.gl2Pass()->addFilterKey(&passFilterKey);
        material.es2Pass()->addFilterKey(&passFilterKey);

        Qt3DCore::QEntity *sceneRoot = buildScene(frameGraphFilter, &material);
        Qt3DRender::TestAspect testAspect(sceneRoot);
        Qt3DRender::Render::MaterialParameterGathererJobPtr gatherer = testAspect.materialGathererJob();

        testAspect.initializeRenderer();

        // THEN
        QCOMPARE(testAspect.nodeManagers()->materialManager()->activeHandles().size(), 1);
        Qt3DRender::Render::RenderPassFilter *backendPassFilter = static_cast<Qt3DRender::Render::RenderPassFilter *>(testAspect.nodeManagers()->frameGraphManager()->lookupNode(frameGraphFilter->id()));
        QVERIFY(backendPassFilter != nullptr);

        // WHEN
        gatherer->setHandles(testAspect.nodeManagers()->materialManager()->activeHandles());
        gatherer->setRenderPassFilter(backendPassFilter);
        gatherer->run();

        // THEN
        QCOMPARE(gatherer->materialToPassAndParameter().size(), 1);
    }

    void checkRunSelectAllNoTechniqueFilterPassFilterWithIncompatibleFilter()
    {
        // GIVEN
        Qt3DRender::QRenderPassFilter *frameGraphFilter = renderPassFilter();
        TestMaterial material;

        Qt3DRender::QFilterKey passFilterFilterKey;
        passFilterFilterKey.setName(QStringLiteral("renderingStyle"));
        passFilterFilterKey.setValue(QVariant(QStringLiteral("forward")));

        Qt3DRender::QFilterKey passFilterKey;
        passFilterKey.setName(QStringLiteral("renderingStyle"));
        passFilterKey.setValue(QVariant(QStringLiteral("backward")));

        frameGraphFilter->addMatch(&passFilterFilterKey);

        material.gl3Pass()->addFilterKey(&passFilterKey);
        material.gl2Pass()->addFilterKey(&passFilterKey);
        material.es2Pass()->addFilterKey(&passFilterKey);

        Qt3DCore::QEntity *sceneRoot = buildScene(frameGraphFilter, &material);
        Qt3DRender::TestAspect testAspect(sceneRoot);
        Qt3DRender::Render::MaterialParameterGathererJobPtr gatherer = testAspect.materialGathererJob();

        testAspect.initializeRenderer();

        // THEN
        QCOMPARE(testAspect.nodeManagers()->materialManager()->activeHandles().size(), 1);
        Qt3DRender::Render::RenderPassFilter *backendPassFilter = static_cast<Qt3DRender::Render::RenderPassFilter *>(testAspect.nodeManagers()->frameGraphManager()->lookupNode(frameGraphFilter->id()));
        QVERIFY(backendPassFilter != nullptr);

        // WHEN
        gatherer->setHandles(testAspect.nodeManagers()->materialManager()->activeHandles());
        gatherer->setRenderPassFilter(backendPassFilter);
        gatherer->run();

        // THEN
        QCOMPARE(gatherer->materialToPassAndParameter().size(), 0);
    }
};

QTEST_MAIN(tst_MaterialParameterGatherer)

#include "tst_materialparametergathererjob.moc"
