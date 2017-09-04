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
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DRender/qlevelofdetail.h>
#include <Qt3DRender/private/qlevelofdetail_p.h>

#include "testpostmanarbiter.h"

class tst_QLevelOfDetail: public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QLevelOfDetail *>("lod");

        Qt3DRender::QLevelOfDetail *defaultConstructed = new Qt3DRender::QLevelOfDetail();
        QTest::newRow("defaultConstructed") << defaultConstructed;

        Qt3DRender::QLevelOfDetail *lodDst = new Qt3DRender::QLevelOfDetail();
        QTest::newRow("distLod") << lodDst;

        Qt3DRender::QLevelOfDetail *lodPx = new Qt3DRender::QLevelOfDetail();
        QTest::newRow("pxLod") << lodPx;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QLevelOfDetail *, lod);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(lod);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1);

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DRender::QLevelOfDetailData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QLevelOfDetailData>>(creationChanges.first());
        const Qt3DRender::QLevelOfDetailData &cloneData = creationChangeData->data;

        QCOMPARE(lod->id(), creationChangeData->subjectId());
        QCOMPARE(lod->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(lod->metaObject(), creationChangeData->metaObject());
        QCOMPARE(lod->currentIndex(), cloneData.currentIndex);
        QCOMPARE(lod->thresholdType(), cloneData.thresholdType);
        QCOMPARE(lod->thresholds(), cloneData.thresholds);
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DRender::QLevelOfDetail> lod(new Qt3DRender::QLevelOfDetail());
        arbiter.setArbiterOnNode(lod.data());

        {
            // WHEN
            lod->setThresholdType(Qt3DRender::QLevelOfDetail::ProjectedScreenPixelSizeThreshold);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "thresholdType");
            QCOMPARE(change->value().value<int>(), static_cast<int>(Qt3DRender::QLevelOfDetail::ProjectedScreenPixelSizeThreshold));

            arbiter.events.clear();
        }

        {
            // WHEN
            QVector<qreal> thresholds = {10., 20., 30.};
            lod->setThresholds(thresholds);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "thresholds");
            QCOMPARE(change->value().value<decltype(thresholds)>(), thresholds);

            arbiter.events.clear();
        }
    }
};

QTEST_MAIN(tst_QLevelOfDetail)

#include "tst_qlevelofdetail.moc"
