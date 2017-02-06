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
#include <Qt3DRender/qrendersettings.h>
#include <Qt3DRender/qviewport.h>
#include <Qt3DRender/private/qrendersettings_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QRenderSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        qRegisterMetaType<Qt3DRender::QRenderSettings::RenderPolicy >("RenderPolicy");
        qRegisterMetaType<Qt3DRender::QPickingSettings::PickMethod>("QPickingSettings::PickMethod");
        qRegisterMetaType<Qt3DRender::QPickingSettings::PickResultMode>("QPickingSettings::PickResultMode");
        qRegisterMetaType<Qt3DRender::QPickingSettings::FaceOrientationPickingMode>("QPickingSettings::FaceOrientationPickingMode");
        qRegisterMetaType<Qt3DRender::QFrameGraphNode*>("QFrameGraphNode *");
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QRenderSettings renderSettings;

        // THEN
        QVERIFY(renderSettings.pickingSettings() != nullptr);
        QCOMPARE(renderSettings.renderPolicy(),  Qt3DRender::QRenderSettings::Always);
        QVERIFY(renderSettings.activeFrameGraph() == nullptr);
        QCOMPARE(renderSettings.pickingSettings()->pickMethod(), Qt3DRender::QPickingSettings::BoundingVolumePicking);
        QCOMPARE(renderSettings.pickingSettings()->pickResultMode(), Qt3DRender::QPickingSettings::NearestPick);
        QCOMPARE(renderSettings.pickingSettings()->faceOrientationPickingMode(), Qt3DRender::QPickingSettings::FrontFace);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QRenderSettings renderSettings;
        Qt3DRender::QPickingSettings *pickingSettings = renderSettings.pickingSettings();

        {
            // WHEN
            QSignalSpy spy(&renderSettings, SIGNAL(renderPolicyChanged(RenderPolicy)));
            const Qt3DRender::QRenderSettings::RenderPolicy newValue = Qt3DRender::QRenderSettings::OnDemand;
            renderSettings.setRenderPolicy(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(renderSettings.renderPolicy(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            renderSettings.setRenderPolicy(newValue);

            // THEN
            QCOMPARE(renderSettings.renderPolicy(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&renderSettings, SIGNAL(activeFrameGraphChanged(QFrameGraphNode *)));

            Qt3DRender::QViewport newValue;
            renderSettings.setActiveFrameGraph(&newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(renderSettings.activeFrameGraph(), &newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            renderSettings.setActiveFrameGraph(&newValue);

            // THEN
            QCOMPARE(renderSettings.activeFrameGraph(), &newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(pickingSettings, SIGNAL(pickMethodChanged(QPickingSettings::PickMethod)));
            const Qt3DRender::QPickingSettings::PickMethod newValue = Qt3DRender::QPickingSettings::TrianglePicking;
            pickingSettings->setPickMethod(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(pickingSettings->pickMethod(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            pickingSettings->setPickMethod(newValue);

            // THEN
            QCOMPARE(pickingSettings->pickMethod(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(pickingSettings, SIGNAL(pickResultModeChanged(QPickingSettings::PickResultMode)));
            const Qt3DRender::QPickingSettings::PickResultMode newValue = Qt3DRender::QPickingSettings::AllPicks;
            pickingSettings->setPickResultMode(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(pickingSettings->pickResultMode(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            pickingSettings->setPickResultMode(newValue);

            // THEN
            QCOMPARE(pickingSettings->pickResultMode(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(pickingSettings, SIGNAL(faceOrientationPickingModeChanged(QPickingSettings::FaceOrientationPickingMode)));
            const Qt3DRender::QPickingSettings::FaceOrientationPickingMode newValue = Qt3DRender::QPickingSettings::FrontAndBackFace;
            pickingSettings->setFaceOrientationPickingMode(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(pickingSettings->faceOrientationPickingMode(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            pickingSettings->setFaceOrientationPickingMode(newValue);

            // THEN
            QCOMPARE(pickingSettings->faceOrientationPickingMode(), newValue);
            QCOMPARE(spy.count(), 0);
        }

    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QRenderSettings renderSettings;
        Qt3DRender::QPickingSettings *pickingSettings = renderSettings.pickingSettings();
        Qt3DRender::QViewport frameGraphRoot;

        renderSettings.setRenderPolicy(Qt3DRender::QRenderSettings::OnDemand);
        renderSettings.setActiveFrameGraph(&frameGraphRoot);
        pickingSettings->setPickMethod(Qt3DRender::QPickingSettings::TrianglePicking);
        pickingSettings->setPickResultMode(Qt3DRender::QPickingSettings::AllPicks);
        pickingSettings->setFaceOrientationPickingMode(Qt3DRender::QPickingSettings::FrontAndBackFace);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&renderSettings);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 2); // RenderSettings + PickingSettings (because it's a QNode)

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QRenderSettingsData>>(creationChanges.first());
            const Qt3DRender::QRenderSettingsData cloneData = creationChangeData->data;

            QCOMPARE(renderSettings.pickingSettings()->pickMethod(), cloneData.pickMethod);
            QCOMPARE(renderSettings.pickingSettings()->pickResultMode(), cloneData.pickResultMode);
            QCOMPARE(renderSettings.pickingSettings()->faceOrientationPickingMode(), cloneData.faceOrientationPickingMode);
            QCOMPARE(renderSettings.renderPolicy(), cloneData.renderPolicy);
            QCOMPARE(renderSettings.activeFrameGraph()->id(), cloneData.activeFrameGraphId);
            QCOMPARE(renderSettings.id(), creationChangeData->subjectId());
            QCOMPARE(renderSettings.isEnabled(), true);
            QCOMPARE(renderSettings.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(renderSettings.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        renderSettings.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&renderSettings);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 2); // RenderSettings + PickingSettings (because it's a QNode)

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QRenderSettingsData>>(creationChanges.first());
            const Qt3DRender::QRenderSettingsData cloneData = creationChangeData->data;

            QCOMPARE(renderSettings.pickingSettings()->pickMethod(), cloneData.pickMethod);
            QCOMPARE(renderSettings.pickingSettings()->pickResultMode(), cloneData.pickResultMode);
            QCOMPARE(renderSettings.pickingSettings()->faceOrientationPickingMode(), cloneData.faceOrientationPickingMode);
            QCOMPARE(renderSettings.renderPolicy(), cloneData.renderPolicy);
            QCOMPARE(renderSettings.activeFrameGraph()->id(), cloneData.activeFrameGraphId);
            QCOMPARE(renderSettings.id(), creationChangeData->subjectId());
            QCOMPARE(renderSettings.isEnabled(), false);
            QCOMPARE(renderSettings.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(renderSettings.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkRenderPolicyUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderSettings renderSettings;
        arbiter.setArbiterOnNode(&renderSettings);

        {
            // WHEN
            renderSettings.setRenderPolicy(Qt3DRender::QRenderSettings::OnDemand);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "renderPolicy");
            QCOMPARE(change->value().value<Qt3DRender::QRenderSettings::RenderPolicy>(), renderSettings.renderPolicy());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            renderSettings.setRenderPolicy(Qt3DRender::QRenderSettings::OnDemand);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkActiveFrameGraphUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderSettings renderSettings;
        arbiter.setArbiterOnNode(&renderSettings);
        Qt3DRender::QViewport viewport;

        {
            // WHEN
            renderSettings.setActiveFrameGraph(&viewport);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "activeFrameGraph");
            QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), viewport.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            renderSettings.setActiveFrameGraph(&viewport);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkPickMethodUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderSettings renderSettings;
        Qt3DRender::QPickingSettings *pickingSettings = renderSettings.pickingSettings();

        arbiter.setArbiterOnNode(&renderSettings);

        {
            // WHEN
            pickingSettings->setPickMethod(Qt3DRender::QPickingSettings::TrianglePicking);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "pickMethod");
            QCOMPARE(change->value().value<Qt3DRender::QPickingSettings::PickMethod>(), pickingSettings->pickMethod());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            pickingSettings->setPickMethod(Qt3DRender::QPickingSettings::TrianglePicking);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkPickResultModeUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderSettings renderSettings;
        Qt3DRender::QPickingSettings *pickingSettings = renderSettings.pickingSettings();

        arbiter.setArbiterOnNode(&renderSettings);

        {
            // WHEN
            pickingSettings->setPickResultMode(Qt3DRender::QPickingSettings::AllPicks);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "pickResultMode");
            QCOMPARE(change->value().value<Qt3DRender::QPickingSettings::PickResultMode>(), pickingSettings->pickResultMode());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            pickingSettings->setPickResultMode(Qt3DRender::QPickingSettings::AllPicks);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkFaceOrientationPickingModeUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderSettings renderSettings;
        Qt3DRender::QPickingSettings *pickingSettings = renderSettings.pickingSettings();

        arbiter.setArbiterOnNode(&renderSettings);

        {
            // WHEN
            pickingSettings->setFaceOrientationPickingMode(Qt3DRender::QPickingSettings::FrontAndBackFace);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "faceOrientationPickingMode");
            QCOMPARE(change->value().value<Qt3DRender::QPickingSettings::FaceOrientationPickingMode>(), pickingSettings->faceOrientationPickingMode());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            pickingSettings->setFaceOrientationPickingMode(Qt3DRender::QPickingSettings::FrontAndBackFace);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

};

QTEST_MAIN(tst_QRenderSettings)

#include "tst_qrendersettings.moc"
