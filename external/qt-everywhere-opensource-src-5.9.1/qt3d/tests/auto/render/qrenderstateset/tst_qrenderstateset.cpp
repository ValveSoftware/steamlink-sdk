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

#include <Qt3DRender/qrenderstateset.h>
#include <Qt3DRender/private/qrenderstate_p.h>
#include <Qt3DRender/qrenderstate.h>
#include <Qt3DRender/private/qrenderstateset_p.h>

#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

#include "testpostmanarbiter.h"

class MyStateSet;
class MyStateSetPrivate : public Qt3DRender::QRenderStatePrivate
{
public :
    MyStateSetPrivate()
        : QRenderStatePrivate(Qt3DRender::Render::DepthTestStateMask)
    {}
};


class MyStateSet : public Qt3DRender::QRenderState
{
    Q_OBJECT
public:
    explicit MyStateSet(Qt3DCore::QNode *parent = nullptr)
        : Qt3DRender::QRenderState(*new MyStateSetPrivate(), parent)
    {}

private:
    Q_DECLARE_PRIVATE(MyStateSet)
};

class tst_QRenderStateSet: public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkSaneDefaults()
    {
        QScopedPointer<Qt3DRender::QRenderStateSet> defaultstateSet(new Qt3DRender::QRenderStateSet);
        QVERIFY(defaultstateSet->renderStates().isEmpty());
    }

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QRenderStateSet *>("stateSet");
        QTest::addColumn<QVector<Qt3DRender::QRenderState *> >("states");

        Qt3DRender::QRenderStateSet *defaultConstructed = new Qt3DRender::QRenderStateSet();
        QTest::newRow("defaultConstructed") << defaultConstructed << QVector<Qt3DRender::QRenderState *>();

        Qt3DRender::QRenderStateSet *stateSetWithStates = new Qt3DRender::QRenderStateSet();
        Qt3DRender::QRenderState *state1 = new MyStateSet();
        Qt3DRender::QRenderState *state2 = new MyStateSet();
        QVector<Qt3DRender::QRenderState *> states = QVector<Qt3DRender::QRenderState *>() << state1 << state2;
        stateSetWithStates->addRenderState(state1);
        stateSetWithStates->addRenderState(state2);
        QTest::newRow("stateSetWithStates") << stateSetWithStates << states;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QRenderStateSet*, stateSet);
        QFETCH(QVector<Qt3DRender::QRenderState *>, states);

        // THEN
        QCOMPARE(stateSet->renderStates(), states);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(stateSet);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + states.size());

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DRender::QRenderStateSetData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QRenderStateSetData>>(creationChanges.first());
        const Qt3DRender::QRenderStateSetData &cloneData = creationChangeData->data;

        QCOMPARE(stateSet->id(), creationChangeData->subjectId());
        QCOMPARE(stateSet->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(stateSet->metaObject(), creationChangeData->metaObject());
        QCOMPARE(stateSet->renderStates().count(), cloneData.renderStateIds.count());

        for (int i = 0, m = states.count(); i < m; ++i) {
            Qt3DRender::QRenderState *sOrig = states.at(i);
            QCOMPARE(sOrig->id(), cloneData.renderStateIds.at(i));
        }

        delete stateSet;
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DRender::QRenderStateSet> stateSet(new Qt3DRender::QRenderStateSet());
        arbiter.setArbiterOnNode(stateSet.data());

        // WHEN
        Qt3DRender::QRenderState *state1 = new MyStateSet();
        stateSet->addRenderState(state1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeAddedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(change->propertyName(), "renderState");
        QCOMPARE(change->subjectId(), stateSet->id());
        QCOMPARE(change->addedNodeId(), state1->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();

        // WHEN
        stateSet->addRenderState(state1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 0);

        // WHEN
        stateSet->removeRenderState(state1);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeRemovedChangePtr nodeRemovedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
        QCOMPARE(nodeRemovedChange->propertyName(), "renderState");
        QCOMPARE(nodeRemovedChange->subjectId(), stateSet->id());
        QCOMPARE(nodeRemovedChange->removedNodeId(), state1->id());
        QCOMPARE(nodeRemovedChange->type(), Qt3DCore::PropertyValueRemoved);

        arbiter.events.clear();
    }

    void checkRenderStateBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::QRenderStateSet> renderStateSet(new Qt3DRender::QRenderStateSet);
        {
            // WHEN
            MyStateSet state;
            renderStateSet->addRenderState(&state);

            // THEN
            QCOMPARE(state.parent(), renderStateSet.data());
            QCOMPARE(renderStateSet->renderStates().size(), 1);
        }
        // THEN (Should not crash and parameter be unset)
        QVERIFY(renderStateSet->renderStates().empty());

        {
            // WHEN
            Qt3DRender::QRenderStateSet someOtherStateSet;
            QScopedPointer<Qt3DRender::QRenderState> state(new MyStateSet(&someOtherStateSet));
            renderStateSet->addRenderState(state.data());

            // THEN
            QCOMPARE(state->parent(), &someOtherStateSet);
            QCOMPARE(renderStateSet->renderStates().size(), 1);

            // WHEN
            renderStateSet.reset();
            state.reset();

            // THEN Should not crash when the state is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QRenderStateSet)

#include "tst_qrenderstateset.moc"
