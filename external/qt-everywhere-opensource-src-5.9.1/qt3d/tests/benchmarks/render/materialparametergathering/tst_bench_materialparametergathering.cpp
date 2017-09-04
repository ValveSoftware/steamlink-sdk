/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire
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
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/private/qaspectjobmanager_p.h>

#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DRender/private/qrenderaspect_p.h>
#include <Qt3DRender/private/materialparametergathererjob_p.h>
#include <Qt3DRender/private/technique_p.h>
#include <Qt3DRender/private/techniquemanager_p.h>
#include <Qt3DExtras/qphongmaterial.h>

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

        const auto handles = nodeManagers()->techniqueManager()->activeHandles();
        for (const auto handle: handles) {
            Render::Technique *technique = nodeManagers()->techniqueManager()->data(handle);
            technique->setCompatibleWithRenderer(true);
        }
    }

    ~TestAspect()
    {
        QRenderAspect::onUnregistered();
    }

    Qt3DRender::Render::NodeManagers *nodeManagers() const
    {
        return d_func()->m_renderer->nodeManagers();
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

Qt3DCore::QEntity *buildTestScene(int entityCount)
{
    Qt3DCore::QEntity *root = new Qt3DCore::QEntity();

    for (int i = 0; i < entityCount; ++i) {
        Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(root);
        Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial();
            entity->addComponent(material);
    }

    return root;
}

} // anonymous

class tst_BenchMaterialParameterGathering : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void parameterGathering()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::TestAspect> aspect(new Qt3DRender::TestAspect(buildTestScene(2000)));

        // WHEN
        Qt3DRender::Render::MaterialParameterGathererJobPtr gatheringJob = aspect->materialGathererJob();
        gatheringJob->setHandles(aspect->nodeManagers()->materialManager()->activeHandles());

        QBENCHMARK {
            gatheringJob->run();
        }

        QVERIFY(!gatheringJob->materialToPassAndParameter().empty());
    }
};

QTEST_MAIN(tst_BenchMaterialParameterGathering)

#include "tst_bench_materialparametergathering.moc"
