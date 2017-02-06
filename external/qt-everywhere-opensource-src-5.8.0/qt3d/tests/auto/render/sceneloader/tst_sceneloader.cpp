/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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
#include <qbackendnodetester.h>
#include <Qt3DRender/qsceneloader.h>
#include <Qt3DRender/private/scene_p.h>
#include <Qt3DRender/private/scenemanager_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qbackendnode_p.h>
#include <Qt3DCore/private/qentity_p.h>
#include "testpostmanarbiter.h"
#include "testrenderer.h"

class tst_SceneLoader : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT
private Q_SLOTS:

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        Qt3DRender::Render::Scene sceneLoader;
        Qt3DRender::Render::SceneManager sceneManager;

        // THEN
        QVERIFY(sceneLoader.source().isEmpty());
        QVERIFY(sceneLoader.peerId().isNull());
        QVERIFY(sceneManager.pendingSceneLoaderJobs().isEmpty());


        // GIVEN
        Qt3DRender::QSceneLoader frontendSceneLoader;
        const QUrl newUrl(QStringLiteral("CorvetteMuseum"));
        frontendSceneLoader.setSource(newUrl);

        // WHEN
        sceneLoader.setSceneManager(&sceneManager);
        simulateInitialization(&frontendSceneLoader, &sceneLoader);

        // THEN
        QVERIFY(!sceneLoader.peerId().isNull());
        QCOMPARE(sceneLoader.source(), newUrl);

        // WHEN
        sceneLoader.cleanup();

        // THEN
        QVERIFY(sceneLoader.source().isEmpty());
    }

    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DRender::QSceneLoader frontendSceneLoader;
        frontendSceneLoader.setSource(QUrl(QStringLiteral("CorvetteMuseum")));

        Qt3DRender::Render::Scene sceneLoader;
        Qt3DRender::Render::SceneManager sceneManager;

        // WHEN
        sceneLoader.setSceneManager(&sceneManager);
        simulateInitialization(&frontendSceneLoader, &sceneLoader);

        // THEN
        QCOMPARE(sceneLoader.peerId(), frontendSceneLoader.id());
        QCOMPARE(sceneLoader.source(), frontendSceneLoader.source());
        QVERIFY(!sceneManager.pendingSceneLoaderJobs().isEmpty());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        TestRenderer renderer;
        Qt3DRender::Render::Scene sceneLoader;
        Qt3DRender::Render::SceneManager sceneManager;

        sceneLoader.setRenderer(&renderer);
        sceneLoader.setSceneManager(&sceneManager);

        // THEN
        QVERIFY(sceneManager.pendingSceneLoaderJobs().isEmpty());

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        const QUrl newUrl(QStringLiteral("Bownling_Green_KY"));
        updateChange->setValue(newUrl);
        updateChange->setPropertyName("source");
        sceneLoader.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(sceneLoader.source(), newUrl);
        QVERIFY(!sceneManager.pendingSceneLoaderJobs().isEmpty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(false);
        updateChange->setPropertyName("enabled");
        sceneLoader.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(sceneLoader.isEnabled(), false);
    }

    void checkSubtreeTransmission()
    {
        // GIVEN
        TestRenderer renderer;
        TestArbiter arbiter;
        Qt3DRender::Render::Scene sceneLoader;

        Qt3DCore::QBackendNodePrivate::get(&sceneLoader)->setArbiter(&arbiter);
        sceneLoader.setRenderer(&renderer);

        // WHEN
        Qt3DCore::QEntity subtree;
        sceneLoader.setSceneSubtree(&subtree);

        // THEN
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(arbiter.events.count(), 1);
        QCOMPARE(change->propertyName(), "scene");
        QCOMPARE(change->value().value<Qt3DCore::QEntity *>(), &subtree);

        arbiter.events.clear();
    }

    void checkStatusTransmission()
    {
        // GIVEN
        TestRenderer renderer;
        TestArbiter arbiter;
        Qt3DRender::Render::Scene sceneLoader;

        Qt3DCore::QBackendNodePrivate::get(&sceneLoader)->setArbiter(&arbiter);
        sceneLoader.setRenderer(&renderer);

        // WHEN
        sceneLoader.setStatus(Qt3DRender::QSceneLoader::Ready);

        // THEN
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(arbiter.events.count(), 1);
        QCOMPARE(change->propertyName(), "status");
        QCOMPARE(change->value().value<Qt3DRender::QSceneLoader::Status>(), Qt3DRender::QSceneLoader::Ready);

        arbiter.events.clear();
    }
};

// Note: setSceneSubtree needs a QCoreApplication
QTEST_MAIN(tst_SceneLoader)

#include "tst_sceneloader.moc"
