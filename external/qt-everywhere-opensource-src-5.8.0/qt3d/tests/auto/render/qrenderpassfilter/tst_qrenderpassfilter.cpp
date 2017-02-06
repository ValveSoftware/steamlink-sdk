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

#include <Qt3DRender/qrenderpassfilter.h>
#include <Qt3DRender/private/qrenderpassfilter_p.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qfilterkey.h>

#include <Qt3DCore/QPropertyUpdatedChange>
#include <Qt3DCore/QPropertyNodeAddedChange>
#include <Qt3DCore/QPropertyNodeRemovedChange>

#include "testpostmanarbiter.h"

class tst_QRenderPassFilter: public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkSaneDefaults()
    {
        QScopedPointer<Qt3DRender::QRenderPassFilter> defaultRenderPassFilter(new Qt3DRender::QRenderPassFilter);

        QCOMPARE(defaultRenderPassFilter->matchAny().count(), 0);
        QCOMPARE(defaultRenderPassFilter->parameters().count(), 0);
    }

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QRenderPassFilter *>("renderPassFilter");
        QTest::addColumn<QVector<Qt3DRender::QParameter *> >("parameters");
        QTest::addColumn<QVector<Qt3DRender::QFilterKey *> >("filterKeys");

        Qt3DRender::QRenderPassFilter *defaultConstructed = new Qt3DRender::QRenderPassFilter();
        QTest::newRow("defaultConstructed") << defaultConstructed << QVector<Qt3DRender::QParameter *>() << QVector<Qt3DRender::QFilterKey *>();

        Qt3DRender::QRenderPassFilter *renderPassFilterWithParams = new Qt3DRender::QRenderPassFilter();
        Qt3DRender::QParameter *parameter1 = new Qt3DRender::QParameter(QStringLiteral("displacement"), 454.0f);
        Qt3DRender::QParameter *parameter2 = new Qt3DRender::QParameter(QStringLiteral("torque"), 650);
        QVector<Qt3DRender::QParameter *> params1 = QVector<Qt3DRender::QParameter *>() << parameter1 << parameter2;
        renderPassFilterWithParams->addParameter(parameter1);
        renderPassFilterWithParams->addParameter(parameter2);
        QTest::newRow("renderPassFilterWithParams") << renderPassFilterWithParams << params1 << QVector<Qt3DRender::QFilterKey *>();

        Qt3DRender::QRenderPassFilter *renderPassFilterWithAnnotations = new Qt3DRender::QRenderPassFilter();
        Qt3DRender::QFilterKey *filterKey1 = new Qt3DRender::QFilterKey();
        Qt3DRender::QFilterKey *filterKey2 = new Qt3DRender::QFilterKey();
        filterKey1->setName(QStringLiteral("hasSuperCharger"));
        filterKey1->setValue(true);
        filterKey1->setName(QStringLiteral("hasNitroKit"));
        filterKey1->setValue(false);
        QVector<Qt3DRender::QFilterKey *> filterKeys1 = QVector<Qt3DRender::QFilterKey *>() << filterKey1 << filterKey2;
        renderPassFilterWithAnnotations->addMatch(filterKey1);
        renderPassFilterWithAnnotations->addMatch(filterKey2);
        QTest::newRow("renderPassFilterWithAnnotations") << renderPassFilterWithAnnotations << QVector<Qt3DRender::QParameter *>() << filterKeys1;

        Qt3DRender::QRenderPassFilter *renderPassFilterWithParamsAndAnnotations = new Qt3DRender::QRenderPassFilter();
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
        renderPassFilterWithParamsAndAnnotations->addParameter(parameter3);
        renderPassFilterWithParamsAndAnnotations->addParameter(parameter4);
        renderPassFilterWithParamsAndAnnotations->addMatch(filterKey3);
        renderPassFilterWithParamsAndAnnotations->addMatch(filterKey4);
        QTest::newRow("renderPassFilterWithParamsAndAnnotations") << renderPassFilterWithParamsAndAnnotations << params2 << filterKeys2 ;
    }

    // TODO: Avoid cloning here
    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QRenderPassFilter*, renderPassFilter);
        QFETCH(QVector<Qt3DRender::QParameter *>, parameters);
        QFETCH(QVector<Qt3DRender::QFilterKey *>, filterKeys);

        // THEN
        QCOMPARE(renderPassFilter->parameters(), parameters);
        QCOMPARE(renderPassFilter->matchAny(), filterKeys);

        // WHEN
        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(renderPassFilter);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + renderPassFilter->parameters().size() + renderPassFilter->matchAny().size());

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DRender::QRenderPassFilterData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QRenderPassFilterData>>(creationChanges.first());
        const Qt3DRender::QRenderPassFilterData &cloneData = creationChangeData->data;

        // THEN
        QCOMPARE(renderPassFilter->id(), creationChangeData->subjectId());
        QCOMPARE(renderPassFilter->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(renderPassFilter->metaObject(), creationChangeData->metaObject());

        QCOMPARE(renderPassFilter->matchAny().count(), cloneData.matchIds.count());
        QCOMPARE(renderPassFilter->parameters().count(), cloneData.parameterIds.count());

        // TO DO: Add unit tests for QParameter / QFilterKey

        delete renderPassFilter;
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DRender::QRenderPassFilter> renderPassFilter(new Qt3DRender::QRenderPassFilter());
        arbiter.setArbiterOnNode(renderPassFilter.data());

        // WHEN
        Qt3DRender::QParameter *param1 = new Qt3DRender::QParameter();
        renderPassFilter->addParameter(param1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeAddedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(change->propertyName(), "parameter");
        QCOMPARE(change->subjectId(),renderPassFilter->id());
        QCOMPARE(change->addedNodeId(), param1->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();

        // WHEN
        renderPassFilter->addParameter(param1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 0);

        // WHEN
        renderPassFilter->removeParameter(param1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeRemovedChangePtr nodeRemovedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
        QCOMPARE(nodeRemovedChange->propertyName(), "parameter");
        QCOMPARE(nodeRemovedChange->subjectId(), renderPassFilter->id());
        QCOMPARE(nodeRemovedChange->removedNodeId(), param1->id());
        QCOMPARE(nodeRemovedChange->type(), Qt3DCore::PropertyValueRemoved);

        arbiter.events.clear();

        // WHEN
        Qt3DRender::QFilterKey *filterKey1 = new Qt3DRender::QFilterKey();
        renderPassFilter->addMatch(filterKey1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(change->propertyName(), "match");
        QCOMPARE(change->subjectId(),renderPassFilter->id());
        QCOMPARE(change->addedNodeId(), filterKey1->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();

        // WHEN
        renderPassFilter->addMatch(filterKey1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 0);

        // WHEN
        renderPassFilter->removeMatch(filterKey1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        nodeRemovedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
        QCOMPARE(nodeRemovedChange->propertyName(), "match");
        QCOMPARE(nodeRemovedChange->subjectId(), renderPassFilter->id());
        QCOMPARE(nodeRemovedChange->removedNodeId(), filterKey1->id());
        QCOMPARE(nodeRemovedChange->type(), Qt3DCore::PropertyValueRemoved);

        arbiter.events.clear();
    }

    void checkParameterBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::QRenderPassFilter> passFilter(new Qt3DRender::QRenderPassFilter);
        {
            // WHEN
            Qt3DRender::QParameter param;
            passFilter->addParameter(&param);

            // THEN
            QCOMPARE(param.parent(), passFilter.data());
            QCOMPARE(passFilter->parameters().size(), 1);
        }
        // THEN (Should not crash and parameter be unset)
        QVERIFY(passFilter->parameters().empty());

        {
            // WHEN
            Qt3DRender::QRenderPassFilter someOtherPassFilter;
            QScopedPointer<Qt3DRender::QParameter> param(new Qt3DRender::QParameter(&someOtherPassFilter));
            passFilter->addParameter(param.data());

            // THEN
            QCOMPARE(param->parent(), &someOtherPassFilter);
            QCOMPARE(passFilter->parameters().size(), 1);

            // WHEN
            passFilter.reset();
            param.reset();

            // THEN Should not crash when the parameter is destroyed (tests for failed removal of destruction helper)
        }
    }

    void checkFilterKeyBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::QRenderPassFilter> passFilter(new Qt3DRender::QRenderPassFilter);
        {
            // WHEN
            Qt3DRender::QFilterKey filterKey;
            passFilter->addMatch(&filterKey);

            // THEN
            QCOMPARE(filterKey.parent(), passFilter.data());
            QCOMPARE(passFilter->matchAny().size(), 1);
        }
        // THEN (Should not crash and parameter be unset)
        QVERIFY(passFilter->matchAny().empty());

        {
            // WHEN
            Qt3DRender::QRenderPassFilter someOtherPassFilter;
            QScopedPointer<Qt3DRender::QFilterKey> filterKey(new Qt3DRender::QFilterKey(&someOtherPassFilter));
            passFilter->addMatch(filterKey.data());

            // THEN
            QCOMPARE(filterKey->parent(), &someOtherPassFilter);
            QCOMPARE(passFilter->matchAny().size(), 1);

            // WHEN
            passFilter.reset();
            filterKey.reset();

            // THEN Should not crash when the parameter is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QRenderPassFilter)

#include "tst_qrenderpassfilter.moc"
