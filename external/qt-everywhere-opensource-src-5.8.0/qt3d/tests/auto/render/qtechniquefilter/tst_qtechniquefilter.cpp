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
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DRender/qtechniquefilter.h>
#include <Qt3DRender/private/qtechniquefilter_p.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qfilterkey.h>

#include <Qt3DCore/QPropertyUpdatedChange>
#include <Qt3DCore/QPropertyNodeAddedChange>
#include <Qt3DCore/QPropertyNodeRemovedChange>

#include "testpostmanarbiter.h"

class tst_QTechniqueFilter: public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkSaneDefaults()
    {
        QScopedPointer<Qt3DRender::QTechniqueFilter> defaulttechniqueFilter(new Qt3DRender::QTechniqueFilter);

        QCOMPARE(defaulttechniqueFilter->matchAll().count(), 0);
        QCOMPARE(defaulttechniqueFilter->parameters().count(), 0);
    }

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QTechniqueFilter *>("techniqueFilter");
        QTest::addColumn<QVector<Qt3DRender::QParameter *> >("parameters");
        QTest::addColumn<QVector<Qt3DRender::QFilterKey *> >("filterKeys");

        Qt3DRender::QTechniqueFilter *defaultConstructed = new Qt3DRender::QTechniqueFilter();
        QTest::newRow("defaultConstructed") << defaultConstructed << QVector<Qt3DRender::QParameter *>() << QVector<Qt3DRender::QFilterKey *>();

        Qt3DRender::QTechniqueFilter *techniqueFilterWithParams = new Qt3DRender::QTechniqueFilter();
        Qt3DRender::QParameter *parameter1 = new Qt3DRender::QParameter(QStringLiteral("displacement"), 454.0f);
        Qt3DRender::QParameter *parameter2 = new Qt3DRender::QParameter(QStringLiteral("torque"), 650);
        QVector<Qt3DRender::QParameter *> params1 = QVector<Qt3DRender::QParameter *>() << parameter1 << parameter2;
        techniqueFilterWithParams->addParameter(parameter1);
        techniqueFilterWithParams->addParameter(parameter2);
        QTest::newRow("techniqueFilterWithParams") << techniqueFilterWithParams << params1 << QVector<Qt3DRender::QFilterKey *>();

        Qt3DRender::QTechniqueFilter *techniqueFilterWithAnnotations = new Qt3DRender::QTechniqueFilter();
        Qt3DRender::QFilterKey *filterKey1 = new Qt3DRender::QFilterKey();
        Qt3DRender::QFilterKey *filterKey2 = new Qt3DRender::QFilterKey();
        filterKey1->setName(QStringLiteral("hasSuperCharger"));
        filterKey1->setValue(true);
        filterKey1->setName(QStringLiteral("hasNitroKit"));
        filterKey1->setValue(false);
        QVector<Qt3DRender::QFilterKey *> filterKeys1 = QVector<Qt3DRender::QFilterKey *>() << filterKey1 << filterKey2;
        techniqueFilterWithAnnotations->addMatch(filterKey1);
        techniqueFilterWithAnnotations->addMatch(filterKey2);
        QTest::newRow("techniqueFilterWithAnnotations") << techniqueFilterWithAnnotations << QVector<Qt3DRender::QParameter *>() << filterKeys1;

        Qt3DRender::QTechniqueFilter *techniqueFilterWithParamsAndAnnotations = new Qt3DRender::QTechniqueFilter();
        Qt3DRender::QParameter *parameter3 = new Qt3DRender::QParameter(QStringLiteral("displacement"), 383.0f);
        Qt3DRender::QParameter *parameter4 = new Qt3DRender::QParameter(QStringLiteral("torque"), 555);
        Qt3DRender::QFilterKey *filterKey3 = new Qt3DRender::QFilterKey();
        Qt3DRender::QFilterKey *filterKey4 = new Qt3DRender::QFilterKey();
        filterKey3->setName(QStringLiteral("hasSuperCharger"));
        filterKey3->setValue(false);
        filterKey4->setName(QStringLiteral("hasNitroKit"));
        filterKey4->setValue(true);
        QVector<Qt3DRender::QParameter *> params2 = QVector<Qt3DRender::QParameter *>() << parameter3 << parameter4;
        QVector<Qt3DRender::QFilterKey *> filterKeys2 = QVector<Qt3DRender::QFilterKey *>() << filterKey3 << filterKey4;
        techniqueFilterWithParamsAndAnnotations->addParameter(parameter3);
        techniqueFilterWithParamsAndAnnotations->addParameter(parameter4);
        techniqueFilterWithParamsAndAnnotations->addMatch(filterKey3);
        techniqueFilterWithParamsAndAnnotations->addMatch(filterKey4);
        QTest::newRow("techniqueFilterWithParamsAndAnnotations") << techniqueFilterWithParamsAndAnnotations << params2 << filterKeys2;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QTechniqueFilter*, techniqueFilter);
        QFETCH(QVector<Qt3DRender::QParameter *>, parameters);
        QFETCH(QVector<Qt3DRender::QFilterKey *>, filterKeys);

        // THEN
        QCOMPARE(techniqueFilter->parameters(), parameters);
        QCOMPARE(techniqueFilter->matchAll(), filterKeys);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(techniqueFilter);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + parameters.size() + filterKeys.size());

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DRender::QTechniqueFilterData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QTechniqueFilterData>>(creationChanges.first());
        const Qt3DRender::QTechniqueFilterData &cloneData = creationChangeData->data;

        QCOMPARE(techniqueFilter->id(), creationChangeData->subjectId());
        QCOMPARE(techniqueFilter->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(techniqueFilter->metaObject(), creationChangeData->metaObject());

        QCOMPARE(techniqueFilter->matchAll().count(), cloneData.matchIds.count());
        QCOMPARE(techniqueFilter->parameters().count(), cloneData.parameterIds.count());

        for (int i = 0, m = parameters.count(); i < m; ++i) {
            Qt3DRender::QParameter *pOrig = parameters.at(i);
            QCOMPARE(pOrig->id(), cloneData.parameterIds.at(i));
        }

        for (int i = 0, m = filterKeys.count(); i < m; ++i) {
            Qt3DRender::QFilterKey *aOrig = filterKeys.at(i);
            QCOMPARE(aOrig->id(), cloneData.matchIds.at(i));
        }

        delete techniqueFilter;
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DRender::QTechniqueFilter> techniqueFilter(new Qt3DRender::QTechniqueFilter());
        arbiter.setArbiterOnNode(techniqueFilter.data());

        // WHEN
        Qt3DRender::QParameter *param1 = new Qt3DRender::QParameter();
        techniqueFilter->addParameter(param1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeAddedChangePtr nodeAddedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(nodeAddedChange->propertyName(), "parameter");
        QCOMPARE(nodeAddedChange->subjectId(),techniqueFilter->id());
        QCOMPARE(nodeAddedChange->addedNodeId(), param1->id());
        QCOMPARE(nodeAddedChange->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();

        // WHEN
        techniqueFilter->addParameter(param1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 0);

        // WHEN
        techniqueFilter->removeParameter(param1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeRemovedChangePtr nodeRemovedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
        QCOMPARE(nodeRemovedChange->propertyName(), "parameter");
        QCOMPARE(nodeRemovedChange->subjectId(), techniqueFilter->id());
        QCOMPARE(nodeRemovedChange->removedNodeId(), param1->id());
        QCOMPARE(nodeRemovedChange->type(), Qt3DCore::PropertyValueRemoved);

        arbiter.events.clear();

        // WHEN
        Qt3DRender::QFilterKey *filterKey1 = new Qt3DRender::QFilterKey();
        techniqueFilter->addMatch(filterKey1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        nodeAddedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(nodeAddedChange->propertyName(), "matchAll");
        QCOMPARE(nodeAddedChange->subjectId(),techniqueFilter->id());
        QCOMPARE(nodeAddedChange->addedNodeId(), filterKey1->id());
        QCOMPARE(nodeAddedChange->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();

        // WHEN
        techniqueFilter->addMatch(filterKey1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 0);

        // WHEN
        techniqueFilter->removeMatch(filterKey1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        nodeRemovedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
        QCOMPARE(nodeRemovedChange->propertyName(), "matchAll");
        QCOMPARE(nodeRemovedChange->subjectId(), techniqueFilter->id());
        QCOMPARE(nodeRemovedChange->removedNodeId(), filterKey1->id());
        QCOMPARE(nodeRemovedChange->type(), Qt3DCore::PropertyValueRemoved);

        arbiter.events.clear();
    }

    void checkParameterBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::QTechniqueFilter> techniqueFilter(new Qt3DRender::QTechniqueFilter);
        {
            // WHEN
            Qt3DRender::QParameter param;
            techniqueFilter->addParameter(&param);

            // THEN
            QCOMPARE(param.parent(), techniqueFilter.data());
            QCOMPARE(techniqueFilter->parameters().size(), 1);
        }
        // THEN (Should not crash and parameter be unset)
        QVERIFY(techniqueFilter->parameters().empty());

        {
            // WHEN
            Qt3DRender::QTechniqueFilter someOtherTechniqueFilter;
            QScopedPointer<Qt3DRender::QParameter> param(new Qt3DRender::QParameter(&someOtherTechniqueFilter));
            techniqueFilter->addParameter(param.data());

            // THEN
            QCOMPARE(param->parent(), &someOtherTechniqueFilter);
            QCOMPARE(techniqueFilter->parameters().size(), 1);

            // WHEN
            techniqueFilter.reset();
            param.reset();

            // THEN Should not crash when the parameter is destroyed (tests for failed removal of destruction helper)
        }
    }

    void checkFilterKeyBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::QTechniqueFilter> techniqueFilter(new Qt3DRender::QTechniqueFilter);
        {
            // WHEN
            Qt3DRender::QFilterKey filterKey;
            techniqueFilter->addMatch(&filterKey);

            // THEN
            QCOMPARE(filterKey.parent(), techniqueFilter.data());
            QCOMPARE(techniqueFilter->matchAll().size(), 1);
        }
        // THEN (Should not crash and parameter be unset)
        QVERIFY(techniqueFilter->matchAll().empty());

        {
            // WHEN
            Qt3DRender::QTechniqueFilter someOtherTechniqueFilter;
            QScopedPointer<Qt3DRender::QFilterKey> filterKey(new Qt3DRender::QFilterKey(&someOtherTechniqueFilter));
            techniqueFilter->addMatch(filterKey.data());

            // THEN
            QCOMPARE(filterKey->parent(), &someOtherTechniqueFilter);
            QCOMPARE(techniqueFilter->matchAll().size(), 1);

            // WHEN
            techniqueFilter.reset();
            filterKey.reset();

            // THEN Should not crash when the filterKey is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QTechniqueFilter)

#include "tst_qtechniquefilter.moc"
