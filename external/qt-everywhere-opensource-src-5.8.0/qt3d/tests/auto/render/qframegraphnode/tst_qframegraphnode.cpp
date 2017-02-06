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

#include <Qt3DRender/qframegraphnode.h>
#include <Qt3DRender/private/qframegraphnode_p.h>

#include "testpostmanarbiter.h"

class MyFrameGraphNode : public Qt3DRender::QFrameGraphNode
{
    Q_OBJECT
public:
    explicit MyFrameGraphNode(Qt3DCore::QNode *parent = nullptr)
        : QFrameGraphNode(parent)
    {
    }
};

class tst_QFrameGraphNode: public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkSaneDefaults()
    {
        QScopedPointer<Qt3DRender::QFrameGraphNode> defaultFrameGraphNode(new MyFrameGraphNode);

        QVERIFY(defaultFrameGraphNode->isEnabled());
        QVERIFY(defaultFrameGraphNode->parentFrameGraphNode() == nullptr);
    }

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QFrameGraphNode *>("frameGraphNode");
        QTest::addColumn<bool>("enabled");

        Qt3DRender::QFrameGraphNode *defaultConstructed = new MyFrameGraphNode();
        QTest::newRow("defaultConstructed") << defaultConstructed << true;

        Qt3DRender::QFrameGraphNode *disabledFrameGraphNode = new MyFrameGraphNode();
        disabledFrameGraphNode->setEnabled(false);
        QTest::newRow("allBuffers") << disabledFrameGraphNode << false;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QFrameGraphNode *, frameGraphNode);
        QFETCH(bool, enabled);

        // THEN
        QCOMPARE(frameGraphNode->isEnabled(), enabled);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(frameGraphNode);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1);
        const Qt3DCore::QNodeCreatedChangeBasePtr creationChangeData = creationChanges.first();

        // THEN
        QCOMPARE(frameGraphNode->id(), creationChangeData->subjectId());
        QCOMPARE(frameGraphNode->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(frameGraphNode->metaObject(), creationChangeData->metaObject());

        delete frameGraphNode;
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DRender::QFrameGraphNode> frameGraphNode(new MyFrameGraphNode());
        arbiter.setArbiterOnNode(frameGraphNode.data());

        // WHEN
        frameGraphNode->setEnabled(false);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "enabled");
        QCOMPARE(change->subjectId(), frameGraphNode->id());
        QCOMPARE(change->value().toBool(), false);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        frameGraphNode->setEnabled(false);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 0);

        // WHEN
        frameGraphNode->setEnabled(true);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "enabled");
        QCOMPARE(change->subjectId(), frameGraphNode->id());
        QCOMPARE(change->value().toBool(), true);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();
    }

    void checkParentFrameNodeRetrieval()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::QFrameGraphNode> root(new MyFrameGraphNode);

        Qt3DRender::QFrameGraphNode *child1 = new MyFrameGraphNode(root.data());
        Qt3DRender::QFrameGraphNode *child2 = new MyFrameGraphNode(root.data());
        Qt3DCore::QEntity *child3 = new Qt3DCore::QEntity(root.data());

        Qt3DRender::QFrameGraphNode *child11 = new MyFrameGraphNode(child1);
        Qt3DCore::QEntity *child21 = new Qt3DCore::QEntity(child2);
        Qt3DRender::QFrameGraphNode *child31 = new MyFrameGraphNode(child3);

        Qt3DRender::QFrameGraphNode *child211 = new MyFrameGraphNode(child21);

        // WHEN
        QCoreApplication::processEvents();

        // THEN
        QVERIFY(child1->parent() == root.data());
        QVERIFY(child1->parentFrameGraphNode() == root.data());

        QVERIFY(child2->parent() == root.data());
        QVERIFY(child2->parentFrameGraphNode() == root.data());

        QVERIFY(child3->parent() == root.data());


        QVERIFY(child11->parent() == child1);
        QVERIFY(child11->parentFrameGraphNode() == child1);

        QVERIFY(child21->parent() == child2);

        QVERIFY(child31->parent() == child3);
        QVERIFY(child31->parentFrameGraphNode() == root.data());


        QVERIFY(child211->parent() == child21);
        QVERIFY(child211->parentFrameGraphNode() == child2);
    }
};

QTEST_MAIN(tst_QFrameGraphNode)

#include "tst_qframegraphnode.moc"
