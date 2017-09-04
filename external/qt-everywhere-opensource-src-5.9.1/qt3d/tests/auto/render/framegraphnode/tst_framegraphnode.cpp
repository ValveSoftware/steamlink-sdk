/****************************************************************************
**
** Copyright (C) 2015 Paul Lemire paul.lemire350@gmail.com
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

#include <Qt3DRender/private/framegraphnode_p.h>
#include <QtTest/QTest>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include "testrenderer.h"

class MyFrameGraphNode : public Qt3DRender::Render::FrameGraphNode
{
public:
    void setEnabled(bool enabled)
    {
        FrameGraphNode::setEnabled(enabled);
    }
};

class MyQFrameGraphNode : public Qt3DRender::QFrameGraphNode
{
    Q_OBJECT
public:
    MyQFrameGraphNode(Qt3DCore::QNode *parent = nullptr)
        : Qt3DRender::QFrameGraphNode(parent)
    {}
};

class tst_FrameGraphNode : public QObject
{
    Q_OBJECT
public:
    tst_FrameGraphNode(QObject *parent = nullptr)
        : QObject(parent)
    {}

    ~tst_FrameGraphNode()
    {}

    void setIdInternal(Qt3DRender::Render::FrameGraphNode *node, Qt3DCore::QNodeId id)
    {
        Qt3DCore::QBackendNodePrivate::get(node)->m_peerId = id;
    }

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        MyFrameGraphNode n;

        // THEN
        QCOMPARE(n.nodeType(), Qt3DRender::Render::FrameGraphNode::InvalidNodeType);
        QVERIFY(!n.isEnabled());
        QVERIFY(n.peerId().isNull());
        QVERIFY(n.manager() == nullptr);
        QVERIFY(n.parentId().isNull());
        QVERIFY(n.childrenIds().empty());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        QScopedPointer<MyFrameGraphNode> n(new MyFrameGraphNode());

        // WHEN
        n->setEnabled(true);
        // THEN
        QCOMPARE(n->isEnabled(), true);

        // WHEN
        QScopedPointer<Qt3DRender::Render::FrameGraphManager> manager(new Qt3DRender::Render::FrameGraphManager());
        n->setFrameGraphManager(manager.data());
        // THEN
        QCOMPARE(n->manager(), manager.data());

        // WHEN
        const Qt3DCore::QNodeId parentId = Qt3DCore::QNodeId::createId();
        // THEN
        QVERIFY(!parentId.isNull());

        // WHEN
        n->setParentId(parentId);
        // THEN
        QCOMPARE(n->parentId(), parentId);

        // WHEN
        const Qt3DCore::QNodeId childId = Qt3DCore::QNodeId::createId();
        QScopedPointer<Qt3DRender::Render::FrameGraphNode> c(new MyFrameGraphNode());
        setIdInternal(c.data(), childId);
        manager->appendNode(childId, c.data());
        n->appendChildId(childId);
        // THEN
        QCOMPARE(n->childrenIds().count(), 1);

        // WHEN
        n->appendChildId(childId);
        // THEN
        QCOMPARE(n->childrenIds().count(), 1);

        c.take();
    }

    void checkParentChange()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::Render::FrameGraphManager> manager(new Qt3DRender::Render::FrameGraphManager());
        const Qt3DCore::QNodeId parentId = Qt3DCore::QNodeId::createId();
        const Qt3DCore::QNodeId childId = Qt3DCore::QNodeId::createId();
        Qt3DRender::Render::FrameGraphNode *parent1 = new MyFrameGraphNode();
        Qt3DRender::Render::FrameGraphNode *child = new MyFrameGraphNode();

        setIdInternal(parent1, parentId);
        setIdInternal(child, childId);

        manager->appendNode(parentId, parent1);
        manager->appendNode(childId, child);

        parent1->setFrameGraphManager(manager.data());
        child->setFrameGraphManager(manager.data());

        // THEN
        QVERIFY(parent1->childrenIds().isEmpty());
        QVERIFY(child->parentId().isNull());

        // WHEN
        parent1->appendChildId(childId);
        // THEN
        QCOMPARE(child->parentId(), parentId);
        QCOMPARE(child->parent(), parent1);
        QCOMPARE(parent1->childrenIds().count(), 1);
        QCOMPARE(parent1->childrenIds().first(), childId);
        QCOMPARE(parent1->children().count(), parent1->childrenIds().count());
        QCOMPARE(parent1->children().first(), child);

        // WHEN
        parent1->appendChildId(childId);
        // THEN
        QCOMPARE(child->parentId(), parentId);
        QCOMPARE(child->parent(), parent1);
        QCOMPARE(parent1->childrenIds().count(), 1);
        QCOMPARE(parent1->childrenIds().first(), childId);
        QCOMPARE(parent1->children().count(), parent1->childrenIds().count());
        QCOMPARE(parent1->children().first(), child);

        // WHEN
        parent1->removeChildId(childId);
        // THEN
        QVERIFY(child->parentId().isNull());
        QVERIFY(child->parent() == nullptr);
        QCOMPARE(parent1->childrenIds().count(), 0);
        QCOMPARE(parent1->children().count(), parent1->childrenIds().count());
    }

    void checkSetParent()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::Render::FrameGraphManager> manager(new Qt3DRender::Render::FrameGraphManager());
        const Qt3DCore::QNodeId parent1Id = Qt3DCore::QNodeId::createId();
        const Qt3DCore::QNodeId parent2Id = Qt3DCore::QNodeId::createId();
        const Qt3DCore::QNodeId childId = Qt3DCore::QNodeId::createId();

        Qt3DRender::Render::FrameGraphNode *parent1 = new MyFrameGraphNode();
        Qt3DRender::Render::FrameGraphNode *parent2 = new MyFrameGraphNode();
        Qt3DRender::Render::FrameGraphNode *child = new MyFrameGraphNode();

        setIdInternal(parent1, parent1Id);
        setIdInternal(parent2, parent2Id);
        setIdInternal(child, childId);

        manager->appendNode(parent1Id, parent1);
        manager->appendNode(parent2Id, parent2);
        manager->appendNode(childId, child);

        parent1->setFrameGraphManager(manager.data());
        parent2->setFrameGraphManager(manager.data());
        child->setFrameGraphManager(manager.data());

        // THEN
        QCOMPARE(parent1->peerId(), parent1Id);
        QCOMPARE(parent2->peerId(), parent2Id);
        QCOMPARE(child->peerId(), childId);

        QVERIFY(child->parentId().isNull());
        QCOMPARE(parent1->childrenIds().size(), 0);
        QCOMPARE(parent2->childrenIds().size(), 0);

        // WHEN
        child->setParentId(parent1Id);

        // THEN
        QCOMPARE(child->parentId(), parent1Id);
        QCOMPARE(parent1->childrenIds().size(), 1);
        QCOMPARE(parent2->childrenIds().size(), 0);

        // WHEN
        child->setParentId(parent2Id);

        // THEN
        QCOMPARE(child->parentId(), parent2Id);
        QCOMPARE(parent1->childrenIds().size(), 0);
        QCOMPARE(parent2->childrenIds().size(), 1);

        // WHEN
        child->setParentId(Qt3DCore::QNodeId());

        // THEN
        QVERIFY(child->parentId().isNull());
        QCOMPARE(parent1->childrenIds().size(), 0);
        QCOMPARE(parent2->childrenIds().size(), 0);
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        const Qt3DCore::QNodeId fgNode1Id = Qt3DCore::QNodeId::createId();

        Qt3DRender::Render::FrameGraphNode *backendFGNode = new MyFrameGraphNode();
        Qt3DRender::QFrameGraphNode *frontendFGChild = new MyQFrameGraphNode();
        Qt3DRender::Render::FrameGraphNode *backendFGChild = new MyFrameGraphNode();

        QScopedPointer<Qt3DRender::Render::FrameGraphManager> manager(new Qt3DRender::Render::FrameGraphManager());

        TestRenderer renderer;

        backendFGNode->setRenderer(&renderer);

        setIdInternal(backendFGNode, fgNode1Id);
        setIdInternal(backendFGChild, frontendFGChild->id());

        manager->appendNode(fgNode1Id, backendFGNode);
        manager->appendNode(frontendFGChild->id(), backendFGChild);

        backendFGNode->setFrameGraphManager(manager.data());
        backendFGChild->setFrameGraphManager(manager.data());


        // To geneate the type_info in the QNodePrivate of frontendFGChild
        Qt3DCore::QNodeCreatedChangeGenerator generator(frontendFGChild);

        QCOMPARE(backendFGNode->childrenIds().size(), 0);

        {
            // WHEN
            const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), frontendFGChild);
            backendFGNode->sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendFGNode->childrenIds().size(), 1);
            QCOMPARE(backendFGNode->childrenIds().first(), frontendFGChild->id());
        }
        {
            // WHEN
            const auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), frontendFGChild);
            backendFGNode->sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendFGNode->childrenIds().size(), 0);
        }
    }

};

QTEST_MAIN(tst_FrameGraphNode)

#include "tst_framegraphnode.moc"
