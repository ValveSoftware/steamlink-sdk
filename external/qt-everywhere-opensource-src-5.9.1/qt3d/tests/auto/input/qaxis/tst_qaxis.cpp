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
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DInput/QAxis>
#include <Qt3DInput/QAnalogAxisInput>
#include <Qt3DInput/private/qaxis_p.h>

#include <Qt3DCore/QPropertyUpdatedChange>
#include <Qt3DCore/QPropertyNodeAddedChange>
#include <Qt3DCore/QPropertyNodeRemovedChange>

#include "testpostmanarbiter.h"

// We need to call QNode::clone which is protected
// We need to call QAxis::sceneChangeEvent which is protected
// So we sublcass QAxis instead of QObject
class tst_QAxis: public Qt3DInput::QAxis
{
    Q_OBJECT
public:
    tst_QAxis()
    {
    }

private Q_SLOTS:

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DInput::QAxis *>("axis");

        Qt3DInput::QAxis *defaultConstructed = new Qt3DInput::QAxis();
        QTest::newRow("defaultConstructed") << defaultConstructed;

        Qt3DInput::QAxis *namedAxis = new Qt3DInput::QAxis();
        QTest::newRow("namedAxis") << namedAxis;

        Qt3DInput::QAxis *namedAxisWithInputs = new Qt3DInput::QAxis();
        Qt3DInput::QAbstractAxisInput *axisInput1 = new Qt3DInput::QAnalogAxisInput();
        Qt3DInput::QAbstractAxisInput *axisInput2 = new Qt3DInput::QAnalogAxisInput();
        Qt3DInput::QAbstractAxisInput *axisInput3 = new Qt3DInput::QAnalogAxisInput();
        namedAxisWithInputs->addInput(axisInput1);
        namedAxisWithInputs->addInput(axisInput2);
        namedAxisWithInputs->addInput(axisInput3);
        QTest::newRow("namedAxisWithInputs") << namedAxisWithInputs;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DInput::QAxis *, axis);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(axis);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + axis->inputs().size());

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DInput::QAxisData> creationChangeData =
               qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DInput::QAxisData>>(creationChanges.first());
        const Qt3DInput::QAxisData &cloneData = creationChangeData->data;

        // THEN
        QCOMPARE(axis->id(), creationChangeData->subjectId());
        QCOMPARE(axis->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(axis->metaObject(), creationChangeData->metaObject());
        QCOMPARE(axis->inputs().count(), cloneData.inputIds.count());

        for (int i = 0, m = axis->inputs().count(); i < m; ++i)
            QCOMPARE(axis->inputs().at(i)->id(), cloneData.inputIds.at(i));
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DInput::QAxis> axis(new Qt3DInput::QAxis());
        arbiter.setArbiterOnNode(axis.data());

        // WHEN
        Qt3DInput::QAbstractAxisInput *input = new Qt3DInput::QAnalogAxisInput();
        axis->addInput(input);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeAddedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(change->propertyName(), "input");
        QCOMPARE(change->addedNodeId(), input->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();

        // WHEN
        axis->removeInput(input);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeRemovedChangePtr nodeRemovedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
        QCOMPARE(nodeRemovedChange->propertyName(), "input");
        QCOMPARE(nodeRemovedChange->removedNodeId(), input->id());
        QCOMPARE(nodeRemovedChange->type(), Qt3DCore::PropertyValueRemoved);

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
    }

    void checkAxisInputBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DInput::QAxis> axis(new Qt3DInput::QAxis);
        {
            // WHEN
            Qt3DInput::QAnalogAxisInput input;
            axis->addInput(&input);

            // THEN
            QCOMPARE(input.parent(), axis.data());
            QCOMPARE(axis->inputs().size(), 1);
        }
        // THEN (Should not crash and parameter be unset)
        QVERIFY(axis->inputs().empty());

        {
            // WHEN
            Qt3DInput::QAxis someOtherAxis;
            QScopedPointer<Qt3DInput::QAbstractAxisInput> input(new Qt3DInput::QAnalogAxisInput(&someOtherAxis));
            axis->addInput(input.data());

            // THEN
            QCOMPARE(input->parent(), &someOtherAxis);
            QCOMPARE(axis->inputs().size(), 1);

            // WHEN
            axis.reset();
            input.reset();

            // THEN Should not crash when the input is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QAxis)

#include "tst_qaxis.moc"
