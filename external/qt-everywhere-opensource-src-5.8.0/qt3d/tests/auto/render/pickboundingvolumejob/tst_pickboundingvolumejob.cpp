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

#include "qmlscenereader.h"
#include "testpostmanarbiter.h"

#include <QtTest/QTest>
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qtransform.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/private/qaspectjobmanager_p.h>
#include <QtQuick/qquickwindow.h>

#include <Qt3DRender/QCamera>
#include <Qt3DRender/QObjectPicker>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DRender/private/qrenderaspect_p.h>
#include <Qt3DRender/private/pickboundingvolumejob_p.h>
#include <Qt3DRender/private/updatemeshtrianglelistjob_p.h>
#include <Qt3DRender/private/updateworldboundingvolumejob_p.h>
#include <Qt3DRender/private/updateworldtransformjob_p.h>
#include <Qt3DRender/private/expandboundingvolumejob_p.h>
#include <Qt3DRender/private/calcboundingvolumejob_p.h>
#include <Qt3DRender/private/calcgeometrytrianglevolumes_p.h>
#include <Qt3DRender/private/loadbufferjob_p.h>
#include <Qt3DRender/private/buffermanager_p.h>
#include <Qt3DRender/private/geometryrenderermanager_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

class TestAspect : public Qt3DRender::QRenderAspect
{
public:
    TestAspect(Qt3DCore::QNode *root)
        : Qt3DRender::QRenderAspect(Qt3DRender::QRenderAspect::Synchronous)
        , m_sceneRoot(nullptr)
    {
        QRenderAspect::onRegistered();

        const Qt3DCore::QNodeCreatedChangeGenerator generator(root);
        const QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = generator.creationChanges();

        d_func()->setRootAndCreateNodes(qobject_cast<Qt3DCore::QEntity *>(root), creationChanges);

        Render::Entity *rootEntity = nodeManagers()->lookupResource<Render::Entity, Render::EntityManager>(rootEntityId());
        Q_ASSERT(rootEntity);
        m_sceneRoot = rootEntity;
    }

    ~TestAspect()
    {
        QRenderAspect::onUnregistered();
    }

    void onRegistered() { QRenderAspect::onRegistered(); }
    void onUnregistered() { QRenderAspect::onUnregistered(); }

    Qt3DRender::Render::NodeManagers *nodeManagers() const { return d_func()->m_renderer->nodeManagers(); }
    Qt3DRender::Render::FrameGraphNode *frameGraphRoot() const { return d_func()->m_renderer->frameGraphRoot(); }
    Qt3DRender::Render::RenderSettings *renderSettings() const { return d_func()->m_renderer->settings(); }
    Qt3DRender::Render::Entity *sceneRoot() const { return m_sceneRoot; }

private:
    Render::Entity *m_sceneRoot;
};

} // namespace Qt3DRender

QT_END_NAMESPACE

namespace {

void runRequiredJobs(Qt3DRender::TestAspect *test)
{
    Qt3DRender::Render::UpdateWorldTransformJob updateWorldTransform;
    updateWorldTransform.setRoot(test->sceneRoot());
    updateWorldTransform.run();

    // For each buffer
    QVector<Qt3DRender::Render::HBuffer> bufferHandles = test->nodeManagers()->bufferManager()->activeHandles();
    for (auto bufferHandle : bufferHandles) {
        Qt3DRender::Render::LoadBufferJob loadBuffer(bufferHandle);
        loadBuffer.setNodeManager(test->nodeManagers());
        loadBuffer.run();
    }

    Qt3DRender::Render::CalculateBoundingVolumeJob calcBVolume;
    calcBVolume.setManagers(test->nodeManagers());
    calcBVolume.setRoot(test->sceneRoot());
    calcBVolume.run();

    Qt3DRender::Render::UpdateWorldBoundingVolumeJob updateWorldBVolume;
    updateWorldBVolume.setManager(test->nodeManagers()->renderNodesManager());
    updateWorldBVolume.run();

    Qt3DRender::Render::ExpandBoundingVolumeJob expandBVolume;
    expandBVolume.setRoot(test->sceneRoot());
    expandBVolume.run();

    Qt3DRender::Render::UpdateMeshTriangleListJob updateTriangleList;
    updateTriangleList.setManagers(test->nodeManagers());
    updateTriangleList.run();

    // For each geometry id
    QVector<Qt3DRender::Render::HGeometryRenderer> geometryRenderHandles = test->nodeManagers()->geometryRendererManager()->activeHandles();
    for (auto geometryRenderHandle : geometryRenderHandles) {
        Qt3DCore::QNodeId geometryRendererId = test->nodeManagers()->geometryRendererManager()->data(geometryRenderHandle)->peerId();
        Qt3DRender::Render::CalcGeometryTriangleVolumes calcGeometryTriangles(geometryRendererId, test->nodeManagers());
        calcGeometryTriangles.run();
    }
}

void initializePickBoundingVolumeJob(Qt3DRender::Render::PickBoundingVolumeJob *job, Qt3DRender::TestAspect *test)
{
    job->setFrameGraphRoot(test->frameGraphRoot());
    job->setRoot(test->sceneRoot());
    job->setManagers(test->nodeManagers());
    job->setRenderSettings(test->renderSettings());
}

} // anonymous

class tst_PickBoundingVolumeJob : public QObject
{
    Q_OBJECT
private:
    void generateAllPickingSettingsCombinations()
    {
        QTest::addColumn<Qt3DRender::QPickingSettings::PickMethod>("pickMethod");
        QTest::addColumn<Qt3DRender::QPickingSettings::PickResultMode>("pickResultMode");
        QTest::addColumn<Qt3DRender::QPickingSettings::FaceOrientationPickingMode>("faceOrientationPickingMode");

        QTest::newRow("volume, nearest, front") << Qt3DRender::QPickingSettings::BoundingVolumePicking
                                                << Qt3DRender::QPickingSettings::NearestPick
                                                << Qt3DRender::QPickingSettings::FrontFace;

        QTest::newRow("volume, nearest, back") << Qt3DRender::QPickingSettings::BoundingVolumePicking
                                               << Qt3DRender::QPickingSettings::NearestPick
                                               << Qt3DRender::QPickingSettings::BackFace;

        QTest::newRow("volume, nearest, front+back") << Qt3DRender::QPickingSettings::BoundingVolumePicking
                                                     << Qt3DRender::QPickingSettings::NearestPick
                                                     << Qt3DRender::QPickingSettings::FrontAndBackFace;

        QTest::newRow("volume, all, front") << Qt3DRender::QPickingSettings::BoundingVolumePicking
                                            << Qt3DRender::QPickingSettings::AllPicks
                                            << Qt3DRender::QPickingSettings::FrontFace;

        QTest::newRow("volume, all, back") << Qt3DRender::QPickingSettings::BoundingVolumePicking
                                           << Qt3DRender::QPickingSettings::AllPicks
                                           << Qt3DRender::QPickingSettings::BackFace;

        QTest::newRow("volume, all, front+back") << Qt3DRender::QPickingSettings::BoundingVolumePicking
                                                 << Qt3DRender::QPickingSettings::AllPicks
                                                 << Qt3DRender::QPickingSettings::FrontAndBackFace;

        QTest::newRow("triangle, nearest, front") << Qt3DRender::QPickingSettings::TrianglePicking
                                                  << Qt3DRender::QPickingSettings::NearestPick
                                                  << Qt3DRender::QPickingSettings::FrontFace;

        QTest::newRow("triangle, nearest, back") << Qt3DRender::QPickingSettings::TrianglePicking
                                                 << Qt3DRender::QPickingSettings::NearestPick
                                                 << Qt3DRender::QPickingSettings::BackFace;

        QTest::newRow("triangle, nearest, front+back") << Qt3DRender::QPickingSettings::TrianglePicking
                                                       << Qt3DRender::QPickingSettings::NearestPick
                                                       << Qt3DRender::QPickingSettings::FrontAndBackFace;

        QTest::newRow("triangle, all, front") << Qt3DRender::QPickingSettings::TrianglePicking
                                              << Qt3DRender::QPickingSettings::AllPicks
                                              << Qt3DRender::QPickingSettings::FrontFace;

        QTest::newRow("triangle, all, back") << Qt3DRender::QPickingSettings::TrianglePicking
                                             << Qt3DRender::QPickingSettings::AllPicks
                                             << Qt3DRender::QPickingSettings::BackFace;

        QTest::newRow("triangle, all, front+back") << Qt3DRender::QPickingSettings::TrianglePicking
                                                   << Qt3DRender::QPickingSettings::AllPicks
                                                   << Qt3DRender::QPickingSettings::FrontAndBackFace;
    }

private Q_SLOTS:

    void viewportCameraAreaGather()
    {
        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/testscene_dragenabled.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);
        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));

        // THEN
        QVERIFY(test->frameGraphRoot() != nullptr);
        Qt3DRender::QCamera *camera = root->findChild<Qt3DRender::QCamera *>();
        QVERIFY(camera != nullptr);
        QQuickWindow *window = root->findChild<QQuickWindow *>();
        QVERIFY(camera != nullptr);
        QCOMPARE(window->size(), QSize(600, 600));

        // WHEN
        Qt3DRender::Render::PickingUtils::ViewportCameraAreaGatherer gatherer;
        QVector<Qt3DRender::Render::PickingUtils::ViewportCameraAreaTriplet> results = gatherer.gather(test->frameGraphRoot());

        // THEN
        QCOMPARE(results.size(), 1);
        auto vca = results.first();
        QCOMPARE(vca.area, QSize(600, 600));
        QCOMPARE(vca.cameraId, camera->id());
        QCOMPARE(vca.viewport, QRectF(0.0f, 0.0f, 1.0f, 1.0f));
    }

    void entityGatherer()
    {
        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/testscene_dragenabled.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);
        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));

        // THEN
        QList<Qt3DCore::QEntity *> frontendEntities;
        frontendEntities << qobject_cast<Qt3DCore::QEntity *>(root.data()) << root->findChildren<Qt3DCore::QEntity *>();
        QCOMPARE(frontendEntities.size(), 4);

        // WHEN
        Qt3DRender::Render::PickingUtils::EntityGatherer gatherer(test->nodeManagers()->lookupResource<Qt3DRender::Render::Entity, Qt3DRender::Render::EntityManager>(root->id()));
        QVector<Qt3DRender::Render::Entity *> entities = gatherer.entities();

        // THEN
        QCOMPARE(frontendEntities.size(), entities.size());

        std::sort(frontendEntities.begin(), frontendEntities.end(),
                  [] (Qt3DCore::QEntity *a, Qt3DCore::QEntity *b) { return a->id() > b->id(); });

        std::sort(entities.begin(), entities.end(),
                  [] (Qt3DRender::Render::Entity *a, Qt3DRender::Render::Entity *b) { return a->peerId() > b->peerId(); });

        for (int i = 0, e = frontendEntities.size(); i < e; ++i)
            QCOMPARE(frontendEntities.at(i)->id(), entities.at(i)->peerId());
    }

    void checkCurrentPickerChange_data()
    {
        generateAllPickingSettingsCombinations();
    }

    void checkCurrentPickerChange()
    {
        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/testscene_dragenabled.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);

        QList<Qt3DRender::QRenderSettings *> renderSettings = root->findChildren<Qt3DRender::QRenderSettings *>();
        QCOMPARE(renderSettings.size(), 1);
        Qt3DRender::QPickingSettings *settings = renderSettings.first()->pickingSettings();

        QFETCH(Qt3DRender::QPickingSettings::PickMethod, pickMethod);
        QFETCH(Qt3DRender::QPickingSettings::PickResultMode, pickResultMode);
        QFETCH(Qt3DRender::QPickingSettings::FaceOrientationPickingMode, faceOrientationPickingMode);
        settings->setPickMethod(pickMethod);
        settings->setPickResultMode(pickResultMode);
        settings->setFaceOrientationPickingMode(faceOrientationPickingMode);

        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));

        // Runs Required jobs
        runRequiredJobs(test.data());

        // THEN
        QList<Qt3DRender::QObjectPicker *> pickers = root->findChildren<Qt3DRender::QObjectPicker *>();
        QCOMPARE(pickers.size(), 2);

        Qt3DRender::QObjectPicker *picker1 = nullptr;
        Qt3DRender::QObjectPicker *picker2 = nullptr;

        if (pickers.first()->objectName() == QLatin1String("Picker1")) {
            picker1 = pickers.first();
            picker2 = pickers.last();
        } else {
            picker1 = pickers.last();
            picker2 = pickers.first();
        }

        QCOMPARE(test->renderSettings()->pickMethod(), pickMethod);
        QCOMPARE(test->renderSettings()->pickResultMode(), pickResultMode);
        QCOMPARE(test->renderSettings()->faceOrientationPickingMode(), faceOrientationPickingMode);

        // WHEN
        Qt3DRender::Render::PickBoundingVolumeJob pickBVJob;
        initializePickBoundingVolumeJob(&pickBVJob, test.data());

        // THEN
        QVERIFY(pickBVJob.currentPicker().isNull());

        // WHEN
        QList<QMouseEvent> events;
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonPress, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        bool earlyReturn = !pickBVJob.runHelper();

        // THEN
        QVERIFY(!earlyReturn);
        QVERIFY(!pickBVJob.currentPicker().isNull());
        Qt3DRender::Render::ObjectPicker *backendPicker = test->nodeManagers()->data<Qt3DRender::Render::ObjectPicker, Qt3DRender::Render::ObjectPickerManager>(pickBVJob.currentPicker());
        QVERIFY(backendPicker != nullptr);
        QCOMPARE(backendPicker->peerId(), picker1->id());

        // WHEN
        events.clear();
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonRelease, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN
        QVERIFY(!earlyReturn);
        QVERIFY(pickBVJob.currentPicker().isNull());

        // WHEN
        events.clear();
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonPress, QPointF(390.0f, 300.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN
        QVERIFY(!earlyReturn);
        QVERIFY(!pickBVJob.currentPicker().isNull());
        backendPicker = test->nodeManagers()->data<Qt3DRender::Render::ObjectPicker, Qt3DRender::Render::ObjectPickerManager>(pickBVJob.currentPicker());
        QVERIFY(backendPicker != nullptr);
        QCOMPARE(backendPicker->peerId(), picker2->id());

        // WHEN
        events.clear();
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonRelease, QPointF(390.0f, 300.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN
        QVERIFY(!earlyReturn);
        QVERIFY(pickBVJob.currentPicker().isNull());
    }

    void checkEarlyReturnWhenNoMouseEvents_data()
    {
        generateAllPickingSettingsCombinations();
    }

    void checkEarlyReturnWhenNoMouseEvents()
    {
        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/testscene_dragenabled.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);

        QList<Qt3DRender::QRenderSettings *> renderSettings = root->findChildren<Qt3DRender::QRenderSettings *>();
        QCOMPARE(renderSettings.size(), 1);
        Qt3DRender::QPickingSettings *settings = renderSettings.first()->pickingSettings();

        QFETCH(Qt3DRender::QPickingSettings::PickMethod, pickMethod);
        QFETCH(Qt3DRender::QPickingSettings::PickResultMode, pickResultMode);
        QFETCH(Qt3DRender::QPickingSettings::FaceOrientationPickingMode, faceOrientationPickingMode);
        settings->setPickMethod(pickMethod);
        settings->setPickResultMode(pickResultMode);
        settings->setFaceOrientationPickingMode(faceOrientationPickingMode);

        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));

        // Runs Required jobs
        runRequiredJobs(test.data());

        // THEN
        QList<Qt3DRender::QObjectPicker *> pickers = root->findChildren<Qt3DRender::QObjectPicker *>();
        QCOMPARE(pickers.size(), 2);

        QCOMPARE(test->renderSettings()->pickMethod(), pickMethod);
        QCOMPARE(test->renderSettings()->pickResultMode(), pickResultMode);
        QCOMPARE(test->renderSettings()->faceOrientationPickingMode(), faceOrientationPickingMode);

        // WHEN
        Qt3DRender::Render::PickBoundingVolumeJob pickBVJob;
        initializePickBoundingVolumeJob(&pickBVJob, test.data());

        // THEN
        QVERIFY(pickBVJob.currentPicker().isNull());

        // WHEN
        bool earlyReturn = !pickBVJob.runHelper();

        // THEN
        QVERIFY(earlyReturn);

        // WHEN
        QList<QMouseEvent> events;
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonPress, QPointF(400.0f, 440.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN
        QVERIFY(!earlyReturn);
    }

    void checkEarlyReturnWhenMoveEventsAndNoCurrentPickers_data()
    {
        generateAllPickingSettingsCombinations();
    }

    void checkEarlyReturnWhenMoveEventsAndNoCurrentPickers()
    {
        QSKIP("Disabled following removal of early return checks");

        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/testscene_dragenabled.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);

        QList<Qt3DRender::QRenderSettings *> renderSettings = root->findChildren<Qt3DRender::QRenderSettings *>();
        QCOMPARE(renderSettings.size(), 1);
        Qt3DRender::QPickingSettings *settings = renderSettings.first()->pickingSettings();

        QFETCH(Qt3DRender::QPickingSettings::PickMethod, pickMethod);
        QFETCH(Qt3DRender::QPickingSettings::PickResultMode, pickResultMode);
        QFETCH(Qt3DRender::QPickingSettings::FaceOrientationPickingMode, faceOrientationPickingMode);
        settings->setPickMethod(pickMethod);
        settings->setPickResultMode(pickResultMode);
        settings->setFaceOrientationPickingMode(faceOrientationPickingMode);

        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));

        // Runs Required jobs
        runRequiredJobs(test.data());

        // THEN
        QList<Qt3DRender::QObjectPicker *> pickers = root->findChildren<Qt3DRender::QObjectPicker *>();
        QCOMPARE(pickers.size(), 2);

        QCOMPARE(test->renderSettings()->pickMethod(), pickMethod);
        QCOMPARE(test->renderSettings()->pickResultMode(), pickResultMode);
        QCOMPARE(test->renderSettings()->faceOrientationPickingMode(), faceOrientationPickingMode);

        // WHEN
        Qt3DRender::Render::PickBoundingVolumeJob pickBVJob;
        initializePickBoundingVolumeJob(&pickBVJob, test.data());

        QList<QMouseEvent> events;
        events.push_back(QMouseEvent(QMouseEvent::MouseMove, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);

        // THEN
        QVERIFY(pickBVJob.currentPicker().isNull());

        // WHEN
        const bool earlyReturn = !pickBVJob.runHelper();

        // THEN
        QVERIFY(earlyReturn);
    }

    void checkEarlyReturnWhenMoveEventsAndDragDisabledPickers_data()
    {
        QSKIP("Disabled following removal of early return checks");

        generateAllPickingSettingsCombinations();
    }

    void checkEarlyReturnWhenMoveEventsAndDragDisabledPickers()
    {
        QSKIP("Disabled following removal of early return checks");

        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/testscene_dragdisabled.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);

        QList<Qt3DRender::QRenderSettings *> renderSettings = root->findChildren<Qt3DRender::QRenderSettings *>();
        QCOMPARE(renderSettings.size(), 1);
        Qt3DRender::QPickingSettings *settings = renderSettings.first()->pickingSettings();

        QFETCH(Qt3DRender::QPickingSettings::PickMethod, pickMethod);
        QFETCH(Qt3DRender::QPickingSettings::PickResultMode, pickResultMode);
        QFETCH(Qt3DRender::QPickingSettings::FaceOrientationPickingMode, faceOrientationPickingMode);
        settings->setPickMethod(pickMethod);
        settings->setPickResultMode(pickResultMode);
        settings->setFaceOrientationPickingMode(faceOrientationPickingMode);

        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));

        // Runs Required jobs
        runRequiredJobs(test.data());

        // THEN
        QList<Qt3DRender::QObjectPicker *> pickers = root->findChildren<Qt3DRender::QObjectPicker *>();
        QCOMPARE(pickers.size(), 2);

        QCOMPARE(test->renderSettings()->pickMethod(), pickMethod);
        QCOMPARE(test->renderSettings()->pickResultMode(), pickResultMode);
        QCOMPARE(test->renderSettings()->faceOrientationPickingMode(), faceOrientationPickingMode);

        // WHEN
        Qt3DRender::Render::PickBoundingVolumeJob pickBVJob;
        initializePickBoundingVolumeJob(&pickBVJob, test.data());

        QList<QMouseEvent> events;
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonPress, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        bool earlyReturn = !pickBVJob.runHelper();

        // THEN
        QVERIFY(!pickBVJob.currentPicker().isNull());
        QVERIFY(!earlyReturn);

        // WHEN
        events.clear();
        events.push_back(QMouseEvent(QMouseEvent::MouseMove, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN
        QVERIFY(earlyReturn);
    }

    void checkNoEarlyReturnWhenMoveEventsAndDragEnabledPickers_data()
    {
        generateAllPickingSettingsCombinations();
    }

    void checkNoEarlyReturnWhenMoveEventsAndDragEnabledPickers()
    {
        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/testscene_dragenabled.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);

        QList<Qt3DRender::QRenderSettings *> renderSettings = root->findChildren<Qt3DRender::QRenderSettings *>();
        QCOMPARE(renderSettings.size(), 1);
        Qt3DRender::QPickingSettings *settings = renderSettings.first()->pickingSettings();

        QFETCH(Qt3DRender::QPickingSettings::PickMethod, pickMethod);
        QFETCH(Qt3DRender::QPickingSettings::PickResultMode, pickResultMode);
        QFETCH(Qt3DRender::QPickingSettings::FaceOrientationPickingMode, faceOrientationPickingMode);
        settings->setPickMethod(pickMethod);
        settings->setPickResultMode(pickResultMode);
        settings->setFaceOrientationPickingMode(faceOrientationPickingMode);

        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));

        // Runs Required jobs
        runRequiredJobs(test.data());

        // THEN
        QList<Qt3DRender::QObjectPicker *> pickers = root->findChildren<Qt3DRender::QObjectPicker *>();
        QCOMPARE(pickers.size(), 2);

        QCOMPARE(test->renderSettings()->pickMethod(), pickMethod);
        QCOMPARE(test->renderSettings()->pickResultMode(), pickResultMode);
        QCOMPARE(test->renderSettings()->faceOrientationPickingMode(), faceOrientationPickingMode);

        // WHEN
        Qt3DRender::Render::PickBoundingVolumeJob pickBVJob;
        initializePickBoundingVolumeJob(&pickBVJob, test.data());

        QList<QMouseEvent> events;
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonPress, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        bool earlyReturn = !pickBVJob.runHelper();

        // THEN
        QVERIFY(!pickBVJob.currentPicker().isNull());
        QVERIFY(!earlyReturn);

        // WHEN
        events.clear();
        events.push_back(QMouseEvent(QMouseEvent::MouseMove, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN
        QVERIFY(!earlyReturn);
    }

    void checkEarlyReturnWhenNoProperFrameGraph_data()
    {
        generateAllPickingSettingsCombinations();
    }

    void checkEarlyReturnWhenNoProperFrameGraph()
    {
        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/testscene_improperframegraph.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);

        QList<Qt3DRender::QRenderSettings *> renderSettings = root->findChildren<Qt3DRender::QRenderSettings *>();
        QCOMPARE(renderSettings.size(), 1);
        Qt3DRender::QPickingSettings *settings = renderSettings.first()->pickingSettings();

        QFETCH(Qt3DRender::QPickingSettings::PickMethod, pickMethod);
        QFETCH(Qt3DRender::QPickingSettings::PickResultMode, pickResultMode);
        QFETCH(Qt3DRender::QPickingSettings::FaceOrientationPickingMode, faceOrientationPickingMode);
        settings->setPickMethod(pickMethod);
        settings->setPickResultMode(pickResultMode);
        settings->setFaceOrientationPickingMode(faceOrientationPickingMode);

        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));

        // Runs Required jobs
        runRequiredJobs(test.data());

        // THEN
        QList<Qt3DRender::QObjectPicker *> pickers = root->findChildren<Qt3DRender::QObjectPicker *>();
        QCOMPARE(pickers.size(), 2);

        QCOMPARE(test->renderSettings()->pickMethod(), pickMethod);
        QCOMPARE(test->renderSettings()->pickResultMode(), pickResultMode);
        QCOMPARE(test->renderSettings()->faceOrientationPickingMode(), faceOrientationPickingMode);

        // WHEN
        Qt3DRender::Render::PickBoundingVolumeJob pickBVJob;
        initializePickBoundingVolumeJob(&pickBVJob, test.data());

        QList<QMouseEvent> events;
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonPress, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        const bool earlyReturn = !pickBVJob.runHelper();

        // THEN
        QVERIFY(pickBVJob.currentPicker().isNull());
        QVERIFY(earlyReturn);
    }

    void checkDispatchMouseEvent_data()
    {
        generateAllPickingSettingsCombinations();
    }

    void checkDispatchMouseEvent()
    {
        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/testscene_dragenabled.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);

        QList<Qt3DRender::QRenderSettings *> renderSettings = root->findChildren<Qt3DRender::QRenderSettings *>();
        QCOMPARE(renderSettings.size(), 1);
        Qt3DRender::QPickingSettings *settings = renderSettings.first()->pickingSettings();

        QFETCH(Qt3DRender::QPickingSettings::PickMethod, pickMethod);
        QFETCH(Qt3DRender::QPickingSettings::PickResultMode, pickResultMode);
        QFETCH(Qt3DRender::QPickingSettings::FaceOrientationPickingMode, faceOrientationPickingMode);
        settings->setPickMethod(pickMethod);
        settings->setPickResultMode(pickResultMode);
        settings->setFaceOrientationPickingMode(faceOrientationPickingMode);

        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));
        TestArbiter arbiter;

        // Runs Required jobs
        runRequiredJobs(test.data());

        // THEN
        QList<Qt3DRender::QObjectPicker *> pickers = root->findChildren<Qt3DRender::QObjectPicker *>();
        QCOMPARE(pickers.size(), 2);

        Qt3DRender::QObjectPicker *picker1 = nullptr;
        if (pickers.first()->objectName() == QLatin1String("Picker1"))
            picker1 = pickers.first();
        else
            picker1 = pickers.last();

        Qt3DRender::Render::ObjectPicker *backendPicker1 = test->nodeManagers()->objectPickerManager()->lookupResource(picker1->id());
        QVERIFY(backendPicker1);
        Qt3DCore::QBackendNodePrivate::get(backendPicker1)->setArbiter(&arbiter);

        QCOMPARE(test->renderSettings()->pickMethod(), pickMethod);
        QCOMPARE(test->renderSettings()->pickResultMode(), pickResultMode);
        QCOMPARE(test->renderSettings()->faceOrientationPickingMode(), faceOrientationPickingMode);

        // WHEN -> Pressed on object
        Qt3DRender::Render::PickBoundingVolumeJob pickBVJob;
        initializePickBoundingVolumeJob(&pickBVJob, test.data());

        QList<QMouseEvent> events;
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonPress, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        bool earlyReturn = !pickBVJob.runHelper();

        // THEN -> Pressed
        QVERIFY(!earlyReturn);
        QVERIFY(backendPicker1->isPressed());
        QCOMPARE(arbiter.events.count(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "pressed");

        arbiter.events.clear();

        // WHEN -> Move on same object
        events.clear();
        events.push_back(QMouseEvent(QMouseEvent::MouseMove, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN -> Moved
        QVERIFY(!earlyReturn);
        QVERIFY(backendPicker1->isPressed());
        QCOMPARE(arbiter.events.count(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "moved");

        arbiter.events.clear();

        // WHEN -> Release on object
        events.clear();
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonRelease, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN -> Released + Clicked
        QVERIFY(!earlyReturn);
        QVERIFY(!backendPicker1->isPressed());
        QCOMPARE(arbiter.events.count(), 2);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "clicked");
        change = arbiter.events.last().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "released");

        arbiter.events.clear();

        // WHEN -> Release outside of object
        events.clear();
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonPress, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonRelease, QPointF(0.0f, 0.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN -> Released
        QVERIFY(!earlyReturn);
        QVERIFY(!backendPicker1->isPressed());
        QCOMPARE(arbiter.events.count(), 2);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "pressed");
        change = arbiter.events.last().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "released");

        arbiter.events.clear();
    }

    void checkDispatchHoverEvent_data()
    {
        generateAllPickingSettingsCombinations();
    }

    void checkDispatchHoverEvent()
    {
        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/testscene_dragenabledhoverenabled.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);

        QList<Qt3DRender::QRenderSettings *> renderSettings = root->findChildren<Qt3DRender::QRenderSettings *>();
        QCOMPARE(renderSettings.size(), 1);
        Qt3DRender::QPickingSettings *settings = renderSettings.first()->pickingSettings();

        QFETCH(Qt3DRender::QPickingSettings::PickMethod, pickMethod);
        QFETCH(Qt3DRender::QPickingSettings::PickResultMode, pickResultMode);
        QFETCH(Qt3DRender::QPickingSettings::FaceOrientationPickingMode, faceOrientationPickingMode);
        settings->setPickMethod(pickMethod);
        settings->setPickResultMode(pickResultMode);
        settings->setFaceOrientationPickingMode(faceOrientationPickingMode);

        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));
        TestArbiter arbiter;

        // Runs Required jobs
        runRequiredJobs(test.data());

        // THEN
        QList<Qt3DRender::QObjectPicker *> pickers = root->findChildren<Qt3DRender::QObjectPicker *>();
        QCOMPARE(pickers.size(), 2);

        Qt3DRender::QObjectPicker *picker1 = nullptr;
        if (pickers.first()->objectName() == QLatin1String("Picker1"))
            picker1 = pickers.first();
        else
            picker1 = pickers.last();

        Qt3DRender::Render::ObjectPicker *backendPicker1 = test->nodeManagers()->objectPickerManager()->lookupResource(picker1->id());
        QVERIFY(backendPicker1);
        Qt3DCore::QBackendNodePrivate::get(backendPicker1)->setArbiter(&arbiter);

        QCOMPARE(test->renderSettings()->pickMethod(), pickMethod);
        QCOMPARE(test->renderSettings()->pickResultMode(), pickResultMode);
        QCOMPARE(test->renderSettings()->faceOrientationPickingMode(), faceOrientationPickingMode);

        // WHEN -> Hover on object
        Qt3DRender::Render::PickBoundingVolumeJob pickBVJob;
        initializePickBoundingVolumeJob(&pickBVJob, test.data());

        QList<QMouseEvent> events;
        events.push_back(QMouseEvent(QMouseEvent::HoverMove, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        bool earlyReturn = !pickBVJob.runHelper();

        // THEN -> Entered
        QVERIFY(!earlyReturn);
        QVERIFY(!backendPicker1->isPressed());
        QCOMPARE(arbiter.events.count(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "entered");

        arbiter.events.clear();

        // WHEN -> HoverMove Out
        events.clear();
        events.push_back(QMouseEvent(QEvent::HoverMove, QPointF(20.0f, 40.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN - Exited
        QVERIFY(!earlyReturn);
        QVERIFY(!backendPicker1->isPressed());
        QCOMPARE(arbiter.events.count(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "exited");

        arbiter.events.clear();

        // WHEN -> HoverMove In + Pressed other
        events.clear();
        events.push_back(QMouseEvent(QEvent::HoverMove, QPointF(207.0f, 303.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        events.push_back(QMouseEvent(QEvent::MouseButtonPress, QPointF(0.0f, 0.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN - Entered, Exited
        QVERIFY(!earlyReturn);
        QVERIFY(!backendPicker1->isPressed());
        QCOMPARE(arbiter.events.count(), 2);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "entered");
        change = arbiter.events.last().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "exited");

        arbiter.events.clear();
    }

    void shouldDispatchMouseEventFromChildren_data()
    {
        generateAllPickingSettingsCombinations();
    }

    void shouldDispatchMouseEventFromChildren()
    {
        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/testscene_childentity.qml"));
        QScopedPointer<Qt3DCore::QNode> root(qobject_cast<Qt3DCore::QNode *>(sceneReader.root()));
        QVERIFY(root);

        QList<Qt3DRender::QRenderSettings *> renderSettings = root->findChildren<Qt3DRender::QRenderSettings *>();
        QCOMPARE(renderSettings.size(), 1);
        Qt3DRender::QPickingSettings *settings = renderSettings.first()->pickingSettings();

        QFETCH(Qt3DRender::QPickingSettings::PickMethod, pickMethod);
        QFETCH(Qt3DRender::QPickingSettings::PickResultMode, pickResultMode);
        QFETCH(Qt3DRender::QPickingSettings::FaceOrientationPickingMode, faceOrientationPickingMode);
        settings->setPickMethod(pickMethod);
        settings->setPickResultMode(pickResultMode);
        settings->setFaceOrientationPickingMode(faceOrientationPickingMode);

        QScopedPointer<Qt3DRender::TestAspect> test(new Qt3DRender::TestAspect(root.data()));
        TestArbiter arbiter;

        // Runs Required jobs
        runRequiredJobs(test.data());

        // THEN
        QList<Qt3DRender::QObjectPicker *> pickers = root->findChildren<Qt3DRender::QObjectPicker *>();
        QCOMPARE(pickers.size(), 1);

        Qt3DRender::QObjectPicker *picker = pickers.first();
        QCOMPARE(pickers.first()->objectName(), QLatin1String("Picker"));

        Qt3DRender::Render::ObjectPicker *backendPicker = test->nodeManagers()->objectPickerManager()->lookupResource(picker->id());
        QVERIFY(backendPicker);
        Qt3DCore::QBackendNodePrivate::get(backendPicker)->setArbiter(&arbiter);

        QCOMPARE(test->renderSettings()->pickMethod(), pickMethod);
        QCOMPARE(test->renderSettings()->pickResultMode(), pickResultMode);
        QCOMPARE(test->renderSettings()->faceOrientationPickingMode(), faceOrientationPickingMode);

        // WHEN -> Pressed on object
        Qt3DRender::Render::PickBoundingVolumeJob pickBVJob;
        initializePickBoundingVolumeJob(&pickBVJob, test.data());

        QList<QMouseEvent> events;
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonPress, QPointF(400.0f, 300.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        bool earlyReturn = !pickBVJob.runHelper();

        // THEN -> Pressed
        QVERIFY(!earlyReturn);
        QVERIFY(backendPicker->isPressed());
        QCOMPARE(arbiter.events.count(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "pressed");

        arbiter.events.clear();

        // WHEN -> Move on same object
        events.clear();
        events.push_back(QMouseEvent(QMouseEvent::MouseMove, QPointF(400.0f, 300.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN -> Moved
        QVERIFY(!earlyReturn);
        QVERIFY(backendPicker->isPressed());
        QCOMPARE(arbiter.events.count(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "moved");

        arbiter.events.clear();

        // WHEN -> Release on object
        events.clear();
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonRelease, QPointF(400.0f, 300.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN -> Released + Clicked
        QVERIFY(!earlyReturn);
        QVERIFY(!backendPicker->isPressed());
        QCOMPARE(arbiter.events.count(), 2);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "clicked");
        change = arbiter.events.last().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "released");

        arbiter.events.clear();

        // WHEN -> Release outside of object
        events.clear();
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonPress, QPointF(400.0f, 300.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        events.push_back(QMouseEvent(QMouseEvent::MouseButtonRelease, QPointF(0.0f, 0.0f), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        pickBVJob.setMouseEvents(events);
        earlyReturn = !pickBVJob.runHelper();

        // THEN -> Released
        QVERIFY(!earlyReturn);
        QVERIFY(!backendPicker->isPressed());
        QCOMPARE(arbiter.events.count(), 2);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "pressed");
        change = arbiter.events.last().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "released");

        arbiter.events.clear();
    }
};

QTEST_MAIN(tst_PickBoundingVolumeJob)

#include "tst_pickboundingvolumejob.moc"
