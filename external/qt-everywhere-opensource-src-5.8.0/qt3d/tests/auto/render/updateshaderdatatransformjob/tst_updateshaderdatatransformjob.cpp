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

#include <QtTest/QTest>
#include <Qt3DRender/private/updateshaderdatatransformjob_p.h>
#include <Qt3DRender/private/updateworldtransformjob_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DRender/qshaderdata.h>
#include <Qt3DRender/qcamera.h>
#include <Qt3DRender/private/shaderdata_p.h>
#include <Qt3DRender/private/qrenderaspect_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include "qmlscenereader.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

class TestAspect : public Qt3DRender::QRenderAspect
{
public:
    TestAspect(Qt3DCore::QNode *root)
        : Qt3DRender::QRenderAspect(Qt3DRender::QRenderAspect::Synchronous)
        , m_sceneRoot(nullptr)
    {
        Qt3DRender::QRenderAspect::onRegistered();

        const Qt3DCore::QNodeCreatedChangeGenerator generator(root);
        const QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = generator.creationChanges();

        d_func()->setRootAndCreateNodes(qobject_cast<Qt3DCore::QEntity *>(root), creationChanges);

        Qt3DRender::Render::Entity *rootEntity = nodeManagers()->lookupResource<Qt3DRender::Render::Entity, Render::EntityManager>(rootEntityId());
        Q_ASSERT(rootEntity);
        m_sceneRoot = rootEntity;
    }

    ~TestAspect()
    {
        QRenderAspect::onUnregistered();
    }

    void onRegistered() { Qt3DRender::QRenderAspect::onRegistered(); }
    void onUnregistered() { Qt3DRender::QRenderAspect::onUnregistered(); }

    Qt3DRender::Render::NodeManagers *nodeManagers() const { return d_func()->m_renderer->nodeManagers(); }
    Qt3DRender::Render::FrameGraphNode *frameGraphRoot() const { return d_func()->m_renderer->frameGraphRoot(); }
    Qt3DRender::Render::RenderSettings *renderSettings() const { return d_func()->m_renderer->settings(); }
    Qt3DRender::Render::Entity *sceneRoot() const { return m_sceneRoot; }

private:
    Qt3DRender::Render::Entity *m_sceneRoot;
};

} // Qt3DRender

QT_END_NAMESPACE

namespace {

void runRequiredJobs(Qt3DRender::TestAspect *test)
{
    Qt3DRender::Render::UpdateWorldTransformJob updateWorldTransform;
    updateWorldTransform.setRoot(test->sceneRoot());
    updateWorldTransform.run();
}

struct NodeCollection
{
    explicit NodeCollection(Qt3DRender::TestAspect *aspect, QObject *frontendRoot)
        : shaderData(frontendRoot->findChildren<Qt3DRender::QShaderData *>())
    {
        // THEN
        QCOMPARE(aspect->nodeManagers()->shaderDataManager()->activeHandles().size(), shaderData.size());

        for (const Qt3DRender::QShaderData *s : qAsConst(shaderData)) {
            Qt3DRender::Render::ShaderData *backend = aspect->nodeManagers()->shaderDataManager()->lookupResource(s->id());
            QVERIFY(backend != nullptr);
            backendShaderData.push_back(backend);
        }
    }

    QList<Qt3DRender::QShaderData *> shaderData;
    QVector<Qt3DRender::Render::ShaderData *> backendShaderData;
};

} // anonymous

class tst_UpdateShaderDataTransformJob : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DRender::Render::UpdateShaderDataTransformJob backendUpdateShaderDataTransformJob;

        // THEN
        QVERIFY(backendUpdateShaderDataTransformJob.managers() == nullptr);
    }

    void checkInitializeState()
    {
        // GIVEN
        Qt3DRender::Render::UpdateShaderDataTransformJob backendUpdateShaderDataTransformJob;
        Qt3DRender::Render::NodeManagers managers;

        // WHEN
        backendUpdateShaderDataTransformJob.setManagers(&managers);

        // THEN
        QVERIFY(backendUpdateShaderDataTransformJob.managers() == &managers);
    }

    void checkRunModelToEye()
    {
        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/test_scene_model_to_eye.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);
        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));

        // Properly compute the world transforms
        runRequiredJobs(test.data());

        // WHEN
        Qt3DRender::QCamera *camera = root->findChild<Qt3DRender::QCamera *>();
        const NodeCollection collection(test.data(), root.data());

        // THEN
        QCOMPARE(collection.shaderData.size(), 1);
        QVERIFY(camera != nullptr);

        // WHEN
        Qt3DRender::Render::ShaderData *backendShaderData = collection.backendShaderData.first();

        // THEN
        QCOMPARE(backendShaderData->properties().size(), 2);
        QVERIFY(backendShaderData->properties().contains(QLatin1String("eyePosition")));
        QVERIFY(backendShaderData->properties().contains(QLatin1String("eyePositionTransformed")));

        QCOMPARE(backendShaderData->properties()[QLatin1String("eyePosition")].value<QVector3D>(), QVector3D(1.0f, 1.0f, 1.0f));
        QCOMPARE(backendShaderData->properties()[QLatin1String("eyePositionTransformed")].toInt(), int(Qt3DRender::Render::ShaderData::ModelToEye));

        // WHEN
        Qt3DRender::Render::UpdateShaderDataTransformJob backendUpdateShaderDataTransformJob;
        backendUpdateShaderDataTransformJob.setManagers(test->nodeManagers());
        backendUpdateShaderDataTransformJob.run();

        // THEN
        // See scene file to find translation
        QCOMPARE(backendShaderData->getTransformedProperty(QLatin1String("eyePosition"), camera->viewMatrix()).value<QVector3D>(), camera->viewMatrix() * (QVector3D(1.0f, 1.0f, 1.0f) + QVector3D(0.0f, 5.0f, 0.0f)));
    }

    void checkRunModelToWorld()
    {
        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/test_scene_model_to_world.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);
        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));

        // Properly compute the world transforms
        runRequiredJobs(test.data());

        // WHEN
        Qt3DRender::QCamera *camera = root->findChild<Qt3DRender::QCamera *>();
        const NodeCollection collection(test.data(), root.data());

        // THEN
        QCOMPARE(collection.shaderData.size(), 1);
        QVERIFY(camera != nullptr);

        // WHEN
        Qt3DRender::Render::ShaderData *backendShaderData = collection.backendShaderData.first();

        // THEN
        QCOMPARE(backendShaderData->properties().size(), 2);
        QVERIFY(backendShaderData->properties().contains(QLatin1String("position")));
        QVERIFY(backendShaderData->properties().contains(QLatin1String("positionTransformed")));

        QCOMPARE(backendShaderData->properties()[QLatin1String("position")].value<QVector3D>(), QVector3D(1.0f, 1.0f, 1.0f));
        QCOMPARE(backendShaderData->properties()[QLatin1String("positionTransformed")].toInt(), int(Qt3DRender::Render::ShaderData::ModelToWorld));

        // WHEN
        Qt3DRender::Render::UpdateShaderDataTransformJob backendUpdateShaderDataTransformJob;
        backendUpdateShaderDataTransformJob.setManagers(test->nodeManagers());
        backendUpdateShaderDataTransformJob.run();

        // THEN
        // See scene file to find translation
        QCOMPARE(backendShaderData->getTransformedProperty(QLatin1String("position"), camera->viewMatrix()).value<QVector3D>(), QVector3D(1.0f, 1.0f, 1.0f) + QVector3D(5.0f, 5.0f, 5.0f));
    }
};

QTEST_MAIN(tst_UpdateShaderDataTransformJob)

#include "tst_updateshaderdatatransformjob.moc"
