/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DRender/QLevelOfDetail>
#include <Qt3DRender/QLevelOfDetailBoundingSphere>
#include <Qt3DRender/private/levelofdetail_p.h>
#include <Qt3DRender/private/qlevelofdetail_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qbackendnode_p.h>
#include "testpostmanarbiter.h"
#include "testrenderer.h"

class tst_LevelOfDetail : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT
private Q_SLOTS:

    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DRender::Render::LevelOfDetail renderLod;
        Qt3DRender::QLevelOfDetail lod;

        // WHEN
        simulateInitialization(&lod, &renderLod);

        // THEN
        QCOMPARE(renderLod.peerId(), lod.id());
        QVERIFY(renderLod.camera().isNull() && lod.camera() == nullptr);
        QCOMPARE(renderLod.currentIndex(), lod.currentIndex());
        QCOMPARE(renderLod.thresholdType(), lod.thresholdType());
        QCOMPARE(renderLod.thresholds(), lod.thresholds());
        QCOMPARE(renderLod.center(), lod.volumeOverride().center());
        QCOMPARE(renderLod.radius(), lod.volumeOverride().radius());
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        Qt3DRender::Render::LevelOfDetail renderLod;
        TestRenderer renderer;

        // THEN
        QCOMPARE(renderLod.camera(), Qt3DCore::QNodeId{});
        QCOMPARE(renderLod.currentIndex(), 0);
        QCOMPARE(renderLod.thresholdType(), Qt3DRender::QLevelOfDetail::DistanceToCameraThreshold);
        QVERIFY(renderLod.thresholds().empty());
        QCOMPARE(renderLod.radius(), 1.f);
        QCOMPARE(renderLod.center(), QVector3D{});
        QVERIFY(renderLod.peerId().isNull());

        // GIVEN
        Qt3DRender::QLevelOfDetail lod;
        lod.setThresholdType(Qt3DRender::QLevelOfDetail::ProjectedScreenPixelSizeThreshold);

        // WHEN
        renderLod.setRenderer(&renderer);
        simulateInitialization(&lod, &renderLod);

        // THEN
        QCOMPARE(renderLod.thresholdType(), lod.thresholdType());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        TestRenderer renderer;
        Qt3DRender::Render::LevelOfDetail renderLod;
        renderLod.setRenderer(&renderer);

        // THEN
        QVERIFY(renderLod.thresholdType() != Qt3DRender::QLevelOfDetail::ProjectedScreenPixelSizeThreshold);
        QVERIFY(renderLod.camera().isNull());

        {
            // WHEN
            Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
            updateChange->setValue(static_cast<int>(Qt3DRender::QLevelOfDetail::ProjectedScreenPixelSizeThreshold));
            updateChange->setPropertyName("thresholdType");
            renderLod.sceneChangeEvent(updateChange);

            // THEN
            QCOMPARE(renderLod.thresholdType(), Qt3DRender::QLevelOfDetail::ProjectedScreenPixelSizeThreshold);
            QVERIFY(renderer.dirtyBits() != 0);
        }

        {
            QVector<qreal> thresholds = {20.f, 30.f, 40.f};
            QVariant v;
            v.setValue<decltype(thresholds)>(thresholds);

            // WHEN
            Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
            updateChange->setValue(v);
            updateChange->setPropertyName("thresholds");
            renderLod.sceneChangeEvent(updateChange);

            // THEN
            QCOMPARE(renderLod.thresholds(), thresholds);
        }

        {
            // WHEN
            Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
            Qt3DRender::QLevelOfDetailBoundingSphere sphere(QVector3D(1.0f, 2.0f, 3.0f), 1.0f);
            updateChange->setValue(QVariant::fromValue(sphere));
            updateChange->setPropertyName("volumeOverride");
            renderLod.sceneChangeEvent(updateChange);

            // THEN
            QCOMPARE(renderLod.center(), QVector3D(1., 2., 3.));
        }
    }
};


QTEST_APPLESS_MAIN(tst_LevelOfDetail)

#include "tst_levelofdetail.moc"
