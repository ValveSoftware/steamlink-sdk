/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the S module of the Qt Toolkit.
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

#include <Qt3DRender/qsortcriterion.h>
#include <Qt3DRender/private/qsortcriterion_p.h>

#include "testpostmanarbiter.h"

class tst_QSortCriterion: public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QSortCriterion *>("sortCriterion");
        QTest::addColumn<Qt3DRender::QSortCriterion::SortType>("sortType");

        Qt3DRender::QSortCriterion *defaultConstructed = new Qt3DRender::QSortCriterion();
        QTest::newRow("defaultConstructed") << defaultConstructed << Qt3DRender::QSortCriterion::StateChangeCost;

        Qt3DRender::QSortCriterion *backToFrontSort = new Qt3DRender::QSortCriterion();
        backToFrontSort->setSort(Qt3DRender::QSortCriterion::BackToFront);
        QTest::newRow("backToFrontSort") << backToFrontSort << Qt3DRender::QSortCriterion::BackToFront;

        Qt3DRender::QSortCriterion *materialSort = new Qt3DRender::QSortCriterion();
        materialSort->setSort(Qt3DRender::QSortCriterion::Material);
        QTest::newRow("materialSort") << materialSort << Qt3DRender::QSortCriterion::Material;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QSortCriterion *, sortCriterion);
        QFETCH(Qt3DRender::QSortCriterion::SortType, sortType);

        // THEN
        QCOMPARE(sortCriterion->sort(), sortType);

// TO DO: Add creation change
//        // WHEN
//        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(sortCriterion);
//        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

//        // THEN
//        QCOMPARE(creationChanges.size(), 1);

//        const Qt3DCore::QNodeCreatedChangePtr<Qt3DRender::QCameraSelectorData> creationChangeData =
//                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QSortCriterion>>(creationChanges.first());
//        const Qt3DRender::QCameraSelectorData &cloneData = creationChangeData->data;


//        // THEN
//        QCOMPARE(sortCriterion->id(), creationChangeData->subjectId());
//        QCOMPARE(sortCriterion->isEnabled(), creationChangeData->isNodeEnabled());
//        QCOMPARE(sortCriterion->metaObject(), creationChangeData->metaObject());
//        QCOMPARE(sortCriterion->sort(), cloneData.sort);

        delete sortCriterion;
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::QSortCriterion> sortCriterion(new Qt3DRender::QSortCriterion());
        TestArbiter arbiter(sortCriterion.data());

        // WHEN
        sortCriterion->setSort(Qt3DRender::QSortCriterion::BackToFront);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QNodePropertyChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QNodePropertyChange>();
        QCOMPARE(change->propertyName(), "sort");
        QCOMPARE(change->subjectId(), sortCriterion->id());
        QCOMPARE(change->value().value<Qt3DRender::QSortCriterion::SortType>(), Qt3DRender::QSortCriterion::BackToFront);
        QCOMPARE(change->type(), Qt3DCore::NodeUpdated);

        arbiter.events.clear();

        // WHEN
        sortCriterion->setSort(Qt3DRender::QSortCriterion::BackToFront);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 0);

        // WHEN
        sortCriterion->setSort(Qt3DRender::QSortCriterion::Material);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QNodePropertyChange>();
        QCOMPARE(change->propertyName(), "sort");
        QCOMPARE(change->subjectId(), sortCriterion->id());
        QCOMPARE(change->value().value<Qt3DRender::QSortCriterion::SortType>(), Qt3DRender::QSortCriterion::Material);
        QCOMPARE(change->type(), Qt3DCore::NodeUpdated);

        arbiter.events.clear();
    }
};

QTEST_MAIN(tst_QSortCriterion)

#include "tst_qsortcriterion.moc"
