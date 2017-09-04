/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DCore/private/qpostman_p.h>
#include <Qt3DCore/private/qpostman_p_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/private/qpropertyupdatedchangebase_p.h>
#include "testpostmanarbiter.h"

using namespace Qt3DCore;

namespace {

class NodeChangeReceiver: public QNode
{
public:
    NodeChangeReceiver(QNode *parent = nullptr)
        : QNode(parent)
        , m_hasReceivedChange(false)
    {}

    inline bool hasReceivedChange() const { return m_hasReceivedChange; }

protected:
    void sceneChangeEvent(const QSceneChangePtr &) Q_DECL_OVERRIDE
    {
        m_hasReceivedChange = true;
    }

private:
    bool m_hasReceivedChange;
};

} // anonymous

class tst_QPostman : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkSetScene()
    {
        // GIVEN
        QPostman postman;

        // THEN
        QVERIFY(QPostmanPrivate::get(&postman)->m_scene == nullptr);

        // WHEN
        QScene scene;
        postman.setScene(&scene);

        // THEN
        QCOMPARE(QPostmanPrivate::get(&postman)->m_scene, &scene);
    }

    void checkSceneChangeEvent()
    {
        // GIVEN
        QScopedPointer<QScene> scene(new QScene);
        QPostman postman;
        TestArbiter arbiter;
        QNode rootNode;
        NodeChangeReceiver *receiverNode = new NodeChangeReceiver();

        QNodePrivate::get(&rootNode)->m_scene = scene.data();
        scene->setArbiter(&arbiter);
        postman.setScene(scene.data());
        // Setting the parent (which has a scene) adds the node into the observable lookup
        // table of the scene which is needed by the postman to distribute changes
        static_cast<QNode *>(receiverNode)->setParent(&rootNode);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(receiverNode->hasReceivedChange(), false);
        QCOMPARE(QNodePrivate::get(receiverNode)->m_scene, scene.data());

        // WHEN
        QPropertyUpdatedChangePtr updateChange(new QPropertyUpdatedChange(receiverNode->id()));
        updateChange->setValue(1584);
        updateChange->setPropertyName("someName");
        postman.sceneChangeEvent(updateChange);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(receiverNode->hasReceivedChange(), true);
    }

    void checkNotifyBackend()
    {
        // GIVEN
        QScopedPointer<QScene> scene(new QScene);
        QPostman postman;
        TestArbiter arbiter;

        scene->setArbiter(&arbiter);
        postman.setScene(scene.data());

        // THEN
        QCOMPARE(arbiter.events.size(), 0);

        // WHEN
        QPropertyUpdatedChangePtr updateChange(new QPropertyUpdatedChange(QNodeId()));
        updateChange->setValue(1584);
        updateChange->setPropertyName("someName");
        postman.notifyBackend(updateChange);

        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
    }

    void checkShouldNotifyFrontend()
    {
        // GIVEN
        QScopedPointer<QScene> scene(new QScene);
        QPostman postman;
        TestArbiter arbiter;
        QNode rootNode;
        NodeChangeReceiver *receiverNode = new NodeChangeReceiver();

        QNodePrivate::get(&rootNode)->m_scene = scene.data();
        scene->setArbiter(&arbiter);
        postman.setScene(scene.data());
        // Setting the parent (which has a scene) adds the node into the observable lookup
        // table of the scene which is needed by the postman to distribute changes
        static_cast<QNode *>(receiverNode)->setParent(&rootNode);
        QCoreApplication::processEvents();

        {
            // WHEN
            auto updateChange = QPropertyUpdatedChangePtr::create(receiverNode->id());
            updateChange->setValue(1584);
            updateChange->setPropertyName("someName");


            // THEN -> we do track properties by default QNode::DefaultTrackMode
            // (unless marked as an intermediate change)
            QCOMPARE(postman.shouldNotifyFrontend(updateChange), true);
        }

        {
            // WHEN
            receiverNode->setDefaultPropertyTrackingMode(QNode::TrackAllValues);

            auto updateChange = QPropertyUpdatedChangePtr::create(receiverNode->id());
            updateChange->setValue(1584);
            updateChange->setPropertyName("someName");
            QPropertyUpdatedChangeBasePrivate::get(updateChange.data())->m_isIntermediate
                = true;

            // THEN -> we do track properties marked as intermediate when
            // using TrackAllPropertiesMode
            QCOMPARE(postman.shouldNotifyFrontend(updateChange), true);
        }

        {
            // GIVEN
            receiverNode->setDefaultPropertyTrackingMode(QNode::DontTrackValues);
            receiverNode->setPropertyTracking(QStringLiteral("vette"), Qt3DCore::QNode::TrackAllValues);

            {
                // WHEN
                auto updateChange = QPropertyUpdatedChangePtr::create(receiverNode->id());
                updateChange->setValue(1584);
                updateChange->setPropertyName("someName");
                QPropertyUpdatedChangeBasePrivate::get(updateChange.data())->m_isIntermediate
                    = true;

                // THEN -> we don't track properties by default, unless named when
                // using TrackNamedPropertiesMode
                QCOMPARE(postman.shouldNotifyFrontend(updateChange), false);
            }

            {
                // WHEN
                auto updateChange = QPropertyUpdatedChangePtr::create(receiverNode->id());
                updateChange->setValue(1584);
                updateChange->setPropertyName("vette");
                QPropertyUpdatedChangeBasePrivate::get(updateChange.data())->m_isIntermediate
                    = true;

                // THEN
                QCOMPARE(postman.shouldNotifyFrontend(updateChange), true);
            }
        }

        {
            // GIVEN
            receiverNode->setPropertyTracking(QStringLiteral("vette"), Qt3DCore::QNode::TrackAllValues);
            receiverNode->setDefaultPropertyTrackingMode(QNode::TrackAllValues);

            {
                // WHEN
                auto updateChange = QPropertyUpdatedChangePtr::create(receiverNode->id());
                updateChange->setValue(1584);
                updateChange->setPropertyName("someName");

                // THEN -> we don't track properties by default
                QCOMPARE(postman.shouldNotifyFrontend(updateChange), true);
            }

            {
                // WHEN
                auto updateChange = QPropertyUpdatedChangePtr::create(receiverNode->id());
                updateChange->setValue(1584);
                updateChange->setPropertyName("vette");

                // THEN -> we don't track properties by default
                QCOMPARE(postman.shouldNotifyFrontend(updateChange), true);
            }
        }

        {
            // GIVEN
            receiverNode->clearPropertyTrackings();
            receiverNode->setDefaultPropertyTrackingMode(QNode::TrackFinalValues);

            {
                // WHEN
                auto updateChange = QPropertyUpdatedChangePtr::create(receiverNode->id());
                updateChange->setValue(1584);
                updateChange->setPropertyName("someName");

                // THEN -> we do track properties by default, unless marked as intermediate
                QCOMPARE(postman.shouldNotifyFrontend(updateChange), true);
            }

            {
                // WHEN
                auto addedChange
                    = QPropertyNodeAddedChangePtr::create(receiverNode->id(), receiverNode);

                // THEN -> only QPropertyUpdatedChangePtr are filtered
                QCOMPARE(postman.shouldNotifyFrontend(addedChange), true);
            }
            {
                // WHEN
                auto removedChange
                    = QPropertyNodeRemovedChangePtr::create(receiverNode->id(), receiverNode);

                // THEN -> only QPropertyUpdatedChangePtr are filtered
                QCOMPARE(postman.shouldNotifyFrontend(removedChange), true);
            }
        }

        {
            // GIVEN
            receiverNode->clearPropertyTrackings();
            receiverNode->setDefaultPropertyTrackingMode(QNode::TrackFinalValues);

            {
                // WHEN
                auto updateChange = QPropertyUpdatedChangePtr::create(receiverNode->id());
                updateChange->setValue(1584);
                updateChange->setPropertyName("someName");
                QPropertyUpdatedChangeBasePrivate::get(updateChange.data())->m_isIntermediate
                    = true;

                // THEN -> we don't track intermediate properties by default
                QCOMPARE(postman.shouldNotifyFrontend(updateChange), false);
            }

            {
                // WHEN
                auto updateChange = QPropertyUpdatedChangePtr::create(receiverNode->id());
                updateChange->setValue(1584);
                updateChange->setPropertyName("someName");

                // THEN
                QCOMPARE(postman.shouldNotifyFrontend(updateChange), true);
            }

        }
    }

};

QTEST_MAIN(tst_QPostman)

#include "tst_qpostman.moc"
