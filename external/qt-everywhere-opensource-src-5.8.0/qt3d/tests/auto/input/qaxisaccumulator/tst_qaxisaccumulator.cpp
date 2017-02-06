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

#include <QtTest/QTest>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DInput/qaxis.h>
#include <Qt3DInput/qaxisaccumulator.h>
#include <Qt3DInput/private/qaxisaccumulator_p.h>

#include <Qt3DCore/QPropertyUpdatedChange>
#include <Qt3DCore/QPropertyNodeAddedChange>
#include <Qt3DCore/QPropertyNodeRemovedChange>

#include "testpostmanarbiter.h"

class tst_QAxisAccumulator: public Qt3DInput::QAxisAccumulator
{
    Q_OBJECT
public:
    tst_QAxisAccumulator()
    {
    }

private Q_SLOTS:

    void checkCreationChange_data()
    {
        QTest::addColumn<QSharedPointer<Qt3DInput::QAxisAccumulator>>("accumulator");
        QTest::addColumn<Qt3DInput::QAxis *>("sourceAxis");
        QTest::addColumn<Qt3DInput::QAxisAccumulator::SourceAxisType>("sourceAxisType");
        QTest::addColumn<float>("scale");

        {
            auto accumulator = QSharedPointer<Qt3DInput::QAxisAccumulator>::create();
            QTest::newRow("defaultConstructed")
                    << accumulator
                    << static_cast<Qt3DInput::QAxis *>(nullptr)
                    << Qt3DInput::QAxisAccumulator::Velocity
                    << 1.0f;
        }

        {
            auto accumulator = QSharedPointer<Qt3DInput::QAxisAccumulator>::create();
            Qt3DInput::QAxis *axis = new Qt3DInput::QAxis();
            accumulator->setSourceAxis(axis);
            QTest::newRow("withSourceAxis")
                    << accumulator
                    << axis
                    << Qt3DInput::QAxisAccumulator::Velocity
                    << 1.0f;
        }

        {
            auto accumulator = QSharedPointer<Qt3DInput::QAxisAccumulator>::create();
            accumulator->setSourceAxisType(Qt3DInput::QAxisAccumulator::Acceleration);
            accumulator->setScale(2.5f);
            Qt3DInput::QAxis *axis = new Qt3DInput::QAxis();
            accumulator->setSourceAxis(axis);
            QTest::newRow("accelerationNonUniformScale")
                    << accumulator
                    << axis
                    << Qt3DInput::QAxisAccumulator::Acceleration
                    << 2.5f;
        }
    }

    void checkCreationChange()
    {
        // GIVEN
        QFETCH(QSharedPointer<Qt3DInput::QAxisAccumulator>, accumulator);
        QFETCH(Qt3DInput::QAxis *, sourceAxis);
        QFETCH(Qt3DInput::QAxisAccumulator::SourceAxisType, sourceAxisType);
        QFETCH(float, scale);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(accumulator.data());
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), sourceAxis ? 2 : 1);

        const auto creationChangeData =
               qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DInput::QAxisAccumulatorData>>(creationChanges.first());
        const Qt3DInput::QAxisAccumulatorData &creationData = creationChangeData->data;

        // THEN
        QCOMPARE(accumulator->id(), creationChangeData->subjectId());
        QCOMPARE(accumulator->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(accumulator->metaObject(), creationChangeData->metaObject());
        if (sourceAxis)
            QCOMPARE(sourceAxis->id(), creationData.sourceAxisId);
        QCOMPARE(sourceAxisType, creationData.sourceAxisType);
        QCOMPARE(scale, creationData.scale);
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DInput::QAxisAccumulator> accumulator(new Qt3DInput::QAxisAccumulator());
        arbiter.setArbiterOnNode(accumulator.data());
        Qt3DInput::QAxis *axis = new Qt3DInput::QAxis;

        // WHEN
        accumulator->setSourceAxis(axis);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "sourceAxis");
        QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), axis->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        accumulator->setScale(2.0f);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "scale");
        QCOMPARE(change->value().toFloat(), 2.0f);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        accumulator->setSourceAxisType(Qt3DInput::QAxisAccumulator::Acceleration);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "sourceAxisType");
        QCOMPARE(change->value().value<Qt3DInput::QAxisAccumulator::SourceAxisType>(), Qt3DInput::QAxisAccumulator::Acceleration);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();
    }

    void checkValuePropertyChanged()
    {
        // GIVEN
        QCOMPARE(value(), 0.0f);

        // Note: simulate backend change to frontend
        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr valueChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        valueChange->setPropertyName("value");
        valueChange->setValue(383.0f);
        sceneChangeEvent(valueChange);

        // THEN
        QCOMPARE(value(), 383.0f);
        QCOMPARE(velocity(), 0.0f);

        // WHEN
        valueChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        valueChange->setPropertyName("velocity");
        valueChange->setValue(123.0f);
        sceneChangeEvent(valueChange);

        // THEN
        QCOMPARE(value(), 383.0f);
        QCOMPARE(velocity(), 123.0f);
    }

    void checkAxisInputBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DInput::QAxisAccumulator> accumulator(new Qt3DInput::QAxisAccumulator);
        {
            // WHEN
            Qt3DInput::QAxis axis;
            accumulator->setSourceAxis(&axis);

            // THEN
            QCOMPARE(axis.parent(), accumulator.data());
            QCOMPARE(accumulator->sourceAxis(), &axis);
        }

        // THEN (Should not crash and parameter be unset)
        QVERIFY(accumulator->sourceAxis() == nullptr);

        {
            // WHEN
            Qt3DInput::QAxisAccumulator someOtherAccumulator;
            QScopedPointer<Qt3DInput::QAxis> axis(new Qt3DInput::QAxis(&someOtherAccumulator));
            accumulator->setSourceAxis(axis.data());

            // THEN
            QCOMPARE(axis->parent(), &someOtherAccumulator);
            QCOMPARE(accumulator->sourceAxis(), axis.data());

            // WHEN
            accumulator.reset();
            axis.reset();

            // THEN Should not crash when the input is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QAxisAccumulator)

#include "tst_qaxisaccumulator.moc"
