/****************************************************************************
**
** Copyright (C) 2017 Paul Lemire <paul.lemire350@gmail.com>
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
#include <QObject>
#include <QSignalSpy>
#include <Qt3DAnimation/private/managers_p.h>
#include <Qt3DAnimation/private/clipblendnode_p.h>

namespace {

int deadCount = 0;

class TestClipBlendNode : public Qt3DAnimation::Animation::ClipBlendNode
{
public:
    TestClipBlendNode()
        : Qt3DAnimation::Animation::ClipBlendNode(Qt3DAnimation::Animation::ClipBlendNode::NoneBlendType)
    {}

    ~TestClipBlendNode()
    {
        deadCount += 1;
    }

    inline QVector<Qt3DCore::QNodeId> allDependencyIds() const Q_DECL_OVERRIDE
    {
        return currentDependencyIds();
    }

    QVector<Qt3DCore::QNodeId> currentDependencyIds() const Q_DECL_FINAL
    {
        return QVector<Qt3DCore::QNodeId>();
    }

    double duration() const Q_DECL_FINAL { return 0.0f; }

protected:
    Qt3DAnimation::Animation::ClipResults doBlend(const QVector<Qt3DAnimation::Animation::ClipResults> &) const Q_DECL_FINAL
    {
        return Qt3DAnimation::Animation::ClipResults();
    }
};

} // anonymous

class tst_ClipBlendNodeManager : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkCointainsNode()
    {
        // GIVEN
        Qt3DAnimation::Animation::ClipBlendNodeManager manager;
        const Qt3DCore::QNodeId nodeId = Qt3DCore::QNodeId::createId();

        // THEN
        QVERIFY(manager.containsNode(nodeId) == false);

        // WHEN
        TestClipBlendNode *node = new TestClipBlendNode();
        manager.appendNode(nodeId, node);

        // THEN
        QVERIFY(manager.containsNode(nodeId));
    }

    void checkAppendNode()
    {
        // GIVEN
        Qt3DAnimation::Animation::ClipBlendNodeManager manager;
        const Qt3DCore::QNodeId nodeId = Qt3DCore::QNodeId::createId();
        const Qt3DCore::QNodeId nodeId2 = Qt3DCore::QNodeId::createId();
        TestClipBlendNode *node = new TestClipBlendNode();
        TestClipBlendNode *node2 = new TestClipBlendNode();

        // WHEN
        manager.appendNode(nodeId, node);
        manager.appendNode(nodeId2, node2);

        // THEN
        QVERIFY(manager.containsNode(nodeId));
        QVERIFY(manager.containsNode(nodeId2));
    }

    void checkLookupNode()
    {
        // GIVEN
        Qt3DAnimation::Animation::ClipBlendNodeManager manager;
        const Qt3DCore::QNodeId nodeId = Qt3DCore::QNodeId::createId();
        const Qt3DCore::QNodeId nodeId2 = Qt3DCore::QNodeId::createId();
        TestClipBlendNode *node = new TestClipBlendNode();
        TestClipBlendNode *node2 = new TestClipBlendNode();

        // WHEN
        manager.appendNode(nodeId, node);
        manager.appendNode(nodeId2, node2);
        Qt3DAnimation::Animation::ClipBlendNode *lookedUpNode = manager.lookupNode(nodeId);

        // THEN
        QCOMPARE(lookedUpNode, node);

        // WHEN
        lookedUpNode = manager.lookupNode(nodeId2);

        // THEN
        QCOMPARE(lookedUpNode, node2);

        // WHEN
        lookedUpNode = manager.lookupNode(Qt3DCore::QNodeId::createId());

        // THEN
        QVERIFY(lookedUpNode == nullptr);
    }

    void checkReleaseNode()
    {
        // GIVEN
        Qt3DAnimation::Animation::ClipBlendNodeManager manager;
        const Qt3DCore::QNodeId nodeId = Qt3DCore::QNodeId::createId();
        TestClipBlendNode *node = new TestClipBlendNode();
        deadCount = 0;

        // WHEN
        manager.appendNode(nodeId, node);

        // THEN
        QCOMPARE(deadCount, 0);

        // WHEN
        manager.releaseNode(nodeId);

        // THEN
        QCOMPARE(deadCount, 1);
    }

    void checkDestruction()
    {
        // GIVEN
        const Qt3DCore::QNodeId nodeId = Qt3DCore::QNodeId::createId();
        const Qt3DCore::QNodeId nodeId2 = Qt3DCore::QNodeId::createId();
        TestClipBlendNode *node = new TestClipBlendNode();
        TestClipBlendNode *node2 = new TestClipBlendNode();

        // WHEN
        {
            Qt3DAnimation::Animation::ClipBlendNodeManager manager;
            deadCount = 0;

            // WHEN
            manager.appendNode(nodeId, node);
            manager.appendNode(nodeId2, node2);

            // THEN
            QCOMPARE(deadCount, 0);
        }

        // THEN
        QCOMPARE(deadCount, 2);
    }
};

QTEST_MAIN(tst_ClipBlendNodeManager)

#include "tst_clipblendnodemanager.moc"
