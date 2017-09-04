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

#include <Qt3DInput/QAction>
#include <Qt3DInput/QActionInput>
#include <Qt3DInput/private/qaction_p.h>
#include <Qt3DInput/private/qactioninput_p.h>

#include <Qt3DCore/QPropertyUpdatedChange>
#include <Qt3DCore/QPropertyNodeAddedChange>
#include <Qt3DCore/QPropertyNodeRemovedChange>

#include "testpostmanarbiter.h"

// We need to call QNode::clone which is protected
// We need to call QAction::sceneChangeEvent which is protected
// So we sublcass QNode instead of QObject
class tst_QAction: public Qt3DInput::QAction
{
    Q_OBJECT
public:
    tst_QAction()
    {
    }

private Q_SLOTS:

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DInput::QAction *>("action");

        Qt3DInput::QAction *defaultConstructed = new Qt3DInput::QAction();
        QTest::newRow("defaultConstructed") << defaultConstructed;

        Qt3DInput::QAction *namedaction = new Qt3DInput::QAction();
        QTest::newRow("namedAction") << namedaction;

        Qt3DInput::QAction *namedactionWithInputs = new Qt3DInput::QAction();
        Qt3DInput::QActionInput *actionInput1 = new Qt3DInput::QActionInput();
        Qt3DInput::QActionInput *actionInput2 = new Qt3DInput::QActionInput();
        Qt3DInput::QActionInput *actionInput3 = new Qt3DInput::QActionInput();
        namedactionWithInputs->addInput(actionInput1);
        namedactionWithInputs->addInput(actionInput2);
        namedactionWithInputs->addInput(actionInput3);
        QTest::newRow("namedActionWithInputs") << namedactionWithInputs;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DInput::QAction *, action);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(action);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + action->inputs().size());

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DInput::QActionData> creationChangeData =
               qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DInput::QActionData>>(creationChanges.first());
        const Qt3DInput::QActionData &cloneActionData = creationChangeData->data;

        // THEN
        QCOMPARE(creationChangeData->subjectId(), action->id());
        QCOMPARE(creationChangeData->isNodeEnabled(), action->isEnabled());
        QCOMPARE(creationChangeData->metaObject(), action->metaObject());
        QCOMPARE(creationChangeData->parentId(), action->parentNode() ? action->parentNode()->id() : Qt3DCore::QNodeId());
        QCOMPARE(cloneActionData.inputIds.size(), action->inputs().size());

        const QVector<Qt3DInput::QAbstractActionInput *> &inputs = action->inputs();
        for (int i = 0, m = inputs.size(); i < m; ++i)
            QCOMPARE(cloneActionData.inputIds.at(i), inputs.at(i)->id());
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DInput::QAction> action(new Qt3DInput::QAction());
        arbiter.setArbiterOnNode(action.data());

        // WHEN
        Qt3DInput::QActionInput *input = new Qt3DInput::QActionInput();
        action->addInput(input);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeAddedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(change->propertyName(), "input");
        QCOMPARE(change->addedNodeId(), input->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();

        // WHEN
        action->removeInput(input);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeRemovedChangePtr nodeRemovedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
        QCOMPARE(nodeRemovedChange->propertyName(), "input");
        QCOMPARE(nodeRemovedChange->removedNodeId(), input->id());
        QCOMPARE(nodeRemovedChange->type(), Qt3DCore::PropertyValueRemoved);

        arbiter.events.clear();
    }

    void checkActivePropertyChanged()
    {
        // GIVEN
        QCOMPARE(isActive(), false);

        // Note: simulate backend change to frontend
        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr valueChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        valueChange->setPropertyName("active");
        valueChange->setValue(true);
        sceneChangeEvent(valueChange);

        // THEN
        QCOMPARE(isActive(), true);
    }

    void checkActionInputBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DInput::QAction> action(new Qt3DInput::QAction);
        {
            // WHEN
            Qt3DInput::QActionInput input;
            action->addInput(&input);

            // THEN
            QCOMPARE(input.parent(), action.data());
            QCOMPARE(action->inputs().size(), 1);
        }
        // THEN (Should not crash and parameter be unset)
        QVERIFY(action->inputs().empty());

        {
            // WHEN
            Qt3DInput::QAction someOtherAction;
            QScopedPointer<Qt3DInput::QActionInput> input(new Qt3DInput::QActionInput(&someOtherAction));
            action->addInput(input.data());

            // THEN
            QCOMPARE(input->parent(), &someOtherAction);
            QCOMPARE(action->inputs().size(), 1);

            // WHEN
            action.reset();
            input.reset();

            // THEN Should not crash when the input is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QAction)

#include "tst_qaction.moc"
